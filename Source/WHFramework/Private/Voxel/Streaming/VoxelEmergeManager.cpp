#include "Voxel/Streaming/VoxelEmergeManager.h"

#include "EngineGlobals.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Save/VoxelRegionStore.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	struct FExactOverlayDecodeWork
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ReadLease;
		FVoxelRegionReadView Read;
		TArray<uint8> Bytes;
		FVoxelDeltaDecoder Decoder;
		FVoxelWorldManifest Manifest;
		TSharedPtr<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
		FVoxelGenerationConsumerKey Consumer;
		TAtomic<bool> bWaitingCapacity{false};
		TAtomic<uint64> CapacityVersion{0};

		FVoxelTaskResult Advance(const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;
			if (InCancel.Load())
			{
				Result.bCanceled = true;
				return Result;
			}
			const auto WaitCapacity = [&]
			{
				CapacityVersion.Store(Resources->Snapshot().ReleaseVersion);
				bWaitingCapacity.Store(true);
				Result.bSuccess = Result.bContinue = true;
				Result.Error.Reset();
			};
			bWaitingCapacity.Store(false);
			if (Bytes.IsEmpty())
			{
				if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
					FVoxelRegionStore::ReadWorkingBytes(Read), ReadLease))
				{
					WaitCapacity();
					return Result;
				}
				const auto Status = FVoxelRegionStore::Read(Read, Bytes, Result.Error);
				if (Status != EVoxelRegionRead::Loaded)
				{
					if (Status == EVoxelRegionRead::Missing)
						Result.Error = TEXT("Modified voxel section is missing from its region file");
					return Result;
				}
			}
			if (!Decoder.Advance(Bytes, Manifest, *Registry, Resources.ToSharedRef(), Consumer,
				EVoxelTerrainResourcePool::Staged, Result.PersistentSection, Result.Error))
			{
				if (Result.Error.StartsWith(TEXT("CapacityBlocked:"))) WaitCapacity();
				return Result;
			}
			Result.bSuccess = Result.PersistentSection->Section == Read.Section;
			if (!Result.bSuccess) Result.Error = TEXT("Voxel region section identity mismatch");
			return Result;
		}
	};
}

FVoxelEmergeManager::FVoxelEmergeManager(
	FVoxelWorldRuntime& InRuntime,
	FVoxelTaskScheduler& InScheduler,
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
	const FVoxelRegionStore& InRegionStore,
	const FVoxelWorldManifest& InManifest,
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry)
	: Runtime(InRuntime)
	, Scheduler(InScheduler)
	, Generator(InGenerator)
	, PlanCoordinator(MakeUnique<FVoxelGenerationPlanCoordinator>(InScheduler,
		InConfig, InCache, InRuntime.Epoch()))
	, RegionStore(InRegionStore)
	, Manifest(InManifest)
	, Registry(InRegistry)
{
	AdmissionSelection.BindResources(Scheduler.GetTerrainResources(),
		FVoxelGenerationConsumerKey::Section(Runtime.Epoch(), GenerationOwnerId, 1, {}, 1));
	FVoxelGenerationPlanningServices PlanningServices;
	PlanningServices.Resources = Scheduler.GetRuntimeResources();
	PlanningServices.Ensure = [this](TConstArrayView<FVoxelGenerationPlanDependency> Dependencies, EVoxelWorkClass WorkClass,
		const FVoxelTerrainServicePriority& Priority, const FVoxelGenerationConsumerKey& Consumer)
	{
		return PlanCoordinator->Ensure(Dependencies, WorkClass, Priority, Consumer);
	};
	PlanningServices.Release = [this](const FVoxelGenerationConsumerKey& Consumer) { PlanCoordinator->ReleaseConsumer(Consumer); };
	PlanningServices.Cancel = [this](const FVoxelTaskStamp& Expected)
	{
		Scheduler.CancelMatching([&Expected](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::ProjectBackground && Stamp == Expected;
		});
	};
	PlanningServices.Retain = [this](TConstArrayView<FVoxelGenerationPlanDependency> Dependencies, const FIntPoint& Center)
		-> TSharedPtr<FVoxelGenerationCacheRetentionLease, ESPMode::ThreadSafe>
	{
		return PlanCoordinator->RetainDependencies(Dependencies, Center);
	};
	Generator->SetOverlayPlanningServices(MoveTemp(PlanningServices));
	PlanCoordinator->SetConsumerWakeCallback(
		[this](const FVoxelGenerationConsumerKey& Consumer)
		{
			OnGenerationFactsReady(Consumer);
		});
	Generator->SetTypedOverlayConsumerWakeCallback(
		[this](const FVoxelGenerationConsumerKey& Consumer)
		{
			OnGenerationFactsReady(Consumer);
		});
	Generator->SetOverlayServicePriorityDonation(
		[this](const FVoxelTaskStamp& Stamp, const EVoxelWorkClass InClass, const FVoxelTerrainServicePriority& InPriority)
		{
			if (Stamp.WorldEpoch != Runtime.Epoch() || !Stamp.OwnerId.IsValid() || Stamp.OwnerIncarnation == 0) return;
			Scheduler.UpdateServicePriorities([&](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Current,
				EVoxelWorkClass& WorkClass, FVoxelTerrainServicePriority& Priority)
			{
				if (Kind == EVoxelTaskKind::ProjectBackground && Current == Stamp)
				{
					WorkClass = InClass;
					Priority = InPriority;
				}
			});
		});
	Generator->SetOverlayConsumerWakeCallback(
		[this](const FIntVector& Key)
		{
			WakeCriticalSection(Key);
		});
}

void FVoxelEmergeManager::Tick(const FVoxelInterestSet& InInterest, const uint64 InInterestRevision, const double InNow,
	const TMap<int32, double>& InDataAdmissionLimits, const int32 InMaxBuildsPerFrame, const double InAdmissionMilliseconds,
	const FVoxelTerrainFrameContext* InFrameContext)
{
	if (CurrentInterestRevision != InInterestRevision || CurrentPriorityRevision != InInterest.PriorityRevision)
	{
		RebuildDemand(InInterest, InInterestRevision);
	}
	const double Started = FPlatformTime::Seconds();
	const double Deadline = InFrameContext ? InFrameContext->Deadline(InAdmissionMilliseconds) :
		Started + FMath::Max(0.0, InAdmissionMilliseconds) / 1000.0;
	const int32 BuildLimit = FMath::Clamp(InMaxBuildsPerFrame, 1, 1024);
	int32 Submitted = 0;
	Generator->AdvanceOverlayConsumerWakes(Deadline, 64);
	PlanCoordinator->AdvanceConsumerWakes(Deadline, 64);

	TArray<FVoxelTerrainServiceCandidate> Candidates;
	TArray<FIntVector> CandidateKeys;
	TSet<FIntVector> Captured;
	Candidates.Reserve(128);
	CandidateKeys.Reserve(128);
	const auto CaptureCandidate = [this, &Candidates, &CandidateKeys, &Captured, InNow, &InDataAdmissionLimits](const FIntVector& Key)
	{
		if (Captured.Contains(Key)) return;
		Captured.Add(Key);
		const FVoxelExactDemand* Demand = FindDemand(Key);
		if (!Demand) return;
		const FVoxelSection* Section = Runtime.FindSection(Key);
		if (Section && (Section->Status == EVoxelSectionStatus::DataReady || Section->Status == EVoxelSectionStatus::Failed))
		{
			AdmissionAges.Remove(Key);
			return;
		}
		const auto* Waiting = WaitingPlanSections.Find(Key);
		if (Waiting && InNow < Waiting->RetryAfterSeconds) return;
		const bool bGameplay = Demand->bExact || Demand->bCollision || Demand->bSimulation || Demand->bWarmupData ||
			Demand->bMovementCriticalData || Demand->bMovementCriticalFine;
		const double* Limit = InDataAdmissionLimits.Find(Demand->Priority);
		if (!bGameplay && Limit && Demand->DistanceCells > *Limit) return;
		const uint64 Token = Section ? Section->Stamp.Token : 0;
		FAdmissionServiceAge* Age = AdmissionAges.Find(Key);
		if (Age && Age->SectionToken != 0 && Token != Age->SectionToken)
		{
			AdmissionAges.Remove(Key);
			Age = nullptr;
		}
		if (!Age)
		{
			check(NextAdmissionSequence != MAX_uint64);
			Age = &AdmissionAges.Add(Key, {FPlatformTime::Seconds(), ++NextAdmissionSequence, Token});
		}
		else if (Token != 0) Age->SectionToken = Token;
		FVoxelTerrainServiceCandidate Candidate;
		Candidate.Index = Candidates.Num();
		Candidate.Priority = Demand->ServicePriority;
		Candidate.FirstQueuedSeconds = Age->FirstQueuedSeconds;
		Candidate.Sequence = Age->Sequence;
		Candidate.EstimatedMicroseconds = AdmissionCostEstimateMicroseconds;
		Candidate.bRunnable = true;
		Candidates.Add(Candidate);
		CandidateKeys.Add(Key);
	};
	FIntVector Woken;
	for (int32 Attempt = 0; Attempt < 16 && ReadyCriticalSections.Dequeue(Woken); ++Attempt)
	{
		QueuedCriticalSections.Remove(Woken);
		CaptureCandidate(Woken);
	}
	// 独立数据消费者不修改已提交的 Interest；局部稀疏游标仍进入同一服务选择器。
	for (int32 Step = 0; Step < 32 && !ExternalDataDemands.IsEmpty() && FPlatformTime::Seconds() < Deadline; ++Step)
	{
		ExternalDataCursor %= ExternalDataOrder.GetMaxIndex();
		const int32 Index = ExternalDataCursor++;
		if (ExternalDataOrder.IsValidIndex(Index)) CaptureCandidate(ExternalDataOrder[Index]);
	}
	for (int32 LaneIndex = 0; LaneIndex < 2 && FPlatformTime::Seconds() < Deadline; ++LaneIndex)
	{
		const auto& Lane = AdmissionLanes[LaneIndex];
		int32& Scan = NextAdmissionIndices[LaneIndex];
		for (int32 Attempt = 0; Attempt < FMath::Min(32, Lane.Num()) && FPlatformTime::Seconds() < Deadline; ++Attempt)
		{
			Scan %= Lane.Num();
			CaptureCandidate(OrderedKeys[Lane[Scan++]]);
		}
	}
	for (int32 Attempt = 0; Attempt < Candidates.Num() && Submitted < BuildLimit && FPlatformTime::Seconds() < Deadline; ++Attempt)
	{
		FVoxelTerrainServiceCapacity Capacity;
		Capacity.bAllowOverdueOpportunity = InFrameContext && InFrameContext->IsOverdueOpportunityAvailable();
		FVoxelTerrainServiceCharge Charge;
		const int32 Selected = AdmissionSelection.Select(Candidates, Capacity, FPlatformTime::Seconds(), Charge);
		if (Selected == INDEX_NONE) break;
		if (Charge.bOverdueOpportunity) InFrameContext->ConsumeOverdueOpportunity();
		Candidates[Selected].bRunnable = false;
		const FIntVector Key = CandidateKeys[Selected];
		const double ServiceStarted = FPlatformTime::Seconds();
		const auto* Demand = FindDemand(Key);
		const bool bSubmitted = Demand && RequestSection(Key, *Demand);
		const double Actual = FMath::Max(1.0, (FPlatformTime::Seconds() - ServiceStarted) * 1000000.0);
		verify(AdmissionSelection.Settle(Charge.Serial, Actual));
		AdmissionCostEstimateMicroseconds = FMath::Lerp(AdmissionCostEstimateMicroseconds, Actual, 0.15);
		Submitted += bSubmitted ? 1 : 0;
		if (bSubmitted) AdmissionAges.Remove(Key);
	}
	if (InNow - LastAdmissionDiagnosticTime >= 5.0)
	{
		UE_LOG(LogTemp, Display,
			TEXT("Voxel emerge admission: exact=%d critical=%d ready=%d blocked=%d fallback=%d planPending=%d overlayPending=%d enqueued=%d"),
			CurrentDemand.Num(), CriticalAdmissionKeys.Num(), QueuedCriticalSections.Num(), WaitingPlanSections.Num(), CriticalFallbackCursor,
			PlanPendingAttempts, OverlayPendingAttempts, EnqueuedTasks);
		int32 Reported = 0;
		for (const auto& Pair : WaitingPlanSections)
		{
			const FVoxelExactDemand* Demand = CurrentDemand.Find(Pair.Key);
			if (Reported >= 8)
				break;
			if (!Demand || !IsCriticalDemand(*Demand))
			{
				continue;
			}
			const FVoxelSection* Section = Runtime.FindSection(Pair.Key);
			UE_LOG(LogTemp, Display, TEXT("Voxel critical blocked: section=%s epoch=%llu token=%llu status=%d ageMs=%.2f blocker=%s"), *Pair.Key.ToString(),
				Pair.Value.Stamp.Epoch, Pair.Value.Stamp.Token, Section ? static_cast<int32>(Section->Status) : -1,
				(InNow - Pair.Value.BlockedSinceSeconds) * 1000.0, *Pair.Value.Blocker);
			++Reported;
		}
		LastAdmissionDiagnosticTime = InNow;
		PlanPendingAttempts = 0;
		OverlayPendingAttempts = 0;
		EnqueuedTasks = 0;
	}
}

bool FVoxelEmergeManager::OnTask(
	FVoxelTaskResult&& InResult)
{
	if (InResult.Kind != EVoxelTaskKind::GenerateExactBase &&
		InResult.Kind != EVoxelTaskKind::DecodeOverlay)
	{
		return false;
	}

	FVoxelSection* Section =
		Runtime.FindSection(InResult.Stamp.Section);

	if (!Section)
	{
		return true;
	}

	const FVoxelSectionStamp ExpectedStamp {
		InResult.Stamp.WorldEpoch,
		InResult.Stamp.Token,
		Section->Stamp.RecipeHash
	};

	if (!Runtime.IsCurrentStamp(
		InResult.Stamp.Section,
		ExpectedStamp) || InResult.Stamp.OwnerId != GenerationOwnerId ||
		InResult.Stamp.OwnerIncarnation != Section->Stamp.Token)
	{
		return true;
	}
	if (InResult.Kind == EVoxelTaskKind::GenerateExactBase)
	{
		if (const FWaitingPlanSection* Waiting = WaitingPlanSections.Find(InResult.Stamp.Section);
			Waiting && Waiting->Stamp == ExpectedStamp)
		{
			PlanCoordinator->ReleaseConsumer(Waiting->Consumer);
			Generator->ReleaseOverlayConsumer(Waiting->Consumer);
			WaitingPlanSections.Remove(InResult.Stamp.Section);
		}
	}

	if (InResult.bCanceled)
	{
		WakeCriticalSection(InResult.Stamp.Section);
		return true;
	}

	if (!InResult.bSuccess)
	{
		if (InResult.Kind == EVoxelTaskKind::GenerateExactBase && InResult.Error.StartsWith(TEXT("DependencyNotReady")))
		{
			Section->Status = EVoxelSectionStatus::Allocated;
			Section->Error.Reset();
			if (const FWaitingPlanSection* Waiting = WaitingPlanSections.Find(InResult.Stamp.Section))
			{
				PlanCoordinator->ReleaseConsumer(Waiting->Consumer);
			}
			WaitingPlanSections.Remove(InResult.Stamp.Section);
			WakeCriticalSection(InResult.Stamp.Section);
			return true;
		}
		UE_LOG(LogTemp, Error, TEXT("Voxel section generation failed: section=%s kind=%d error=%s"),
			*InResult.Stamp.Section.ToString(), static_cast<int32>(InResult.Kind), *InResult.Error);
		Section->Status =
			EVoxelSectionStatus::Failed;
		Section->Error = InResult.Error;

		return true;
	}

	FString Error;

	if (InResult.Kind == EVoxelTaskKind::DecodeOverlay)
	{
		if (!InResult.PersistentSection ||
			!Runtime.PublishFinal(
				InResult.Stamp.Section,
				InResult.PersistentSection->Revision,
				InResult.PersistentSection->Blocks,
				InResult.PersistentSection->Entities,
				Error))
		{
			if (InResult.PersistentSection && Error.StartsWith(TEXT("CapacityBlocked:")))
			{
				Section->PendingCapacityVersion = Runtime.GetTerrainResources()->Snapshot().ReleaseVersion;
				*Section->PendingPublish = MoveTemp(InResult);
				return true;
			}
			Section->Status =
				EVoxelSectionStatus::Failed;
			Section->Error = Error;
		}
		else InResult.MarkWorkUsed();

		return true;
	}

	if (!Runtime.PublishBase(
		InResult.Stamp.Section,
		ExpectedStamp,
		MoveTemp(InResult.BaseBlocks),
		Error, InResult.OutputResourceLease))
	{
		if (Error.StartsWith(TEXT("CapacityBlocked:")))
		{
			Section->PendingCapacityVersion = Runtime.GetTerrainResources()->Snapshot().ReleaseVersion;
			*Section->PendingPublish = MoveTemp(InResult);
			return true;
		}
		Section->Status =
			EVoxelSectionStatus::Failed;
		Section->Error = Error;

		return true;
	}

	if (const FVoxelExactDemand* Demand =
		FindDemand(InResult.Stamp.Section))
	{
		InResult.MarkWorkUsed();
		ResolveOverlay(
			*Section,
			InResult.Stamp.Section,
			*Demand);
	}

	return true;
}

void FVoxelEmergeManager::SetRemoteChangeState(
	const FIntVector& InSection,
	const EVoxelSectionChangeState InState)
{
	if (InState == EVoxelSectionChangeState::Unknown)
	{
		RemoteChangeStates.Remove(InSection);
	}
	else
	{
		RemoteChangeStates.Add(
			InSection,
			InState);
	}

	FVoxelSection* Section =
		Runtime.FindSection(InSection);

	const FVoxelExactDemand* Demand =
		FindDemand(InSection);

	if (Section &&
		Demand &&
		Section->Status == EVoxelSectionStatus::BaseReady)
	{
		ResolveOverlay(
			*Section,
			InSection,
			*Demand);
	}
}

FVoxelGenerationDependencyStatus FVoxelEmergeManager::QueryEnvironment(const FIntPoint& InCell, FVoxelEnvironmentSample& OutSample)
{
	if (InCell.X == MAX_int32 || InCell.Y == MAX_int32)
	{
		return { false, true, TEXT("Environment query exceeds world coordinates") };
	}
	if (Generator->TrySampleEnvironment(InCell, OutSample))
	{
		PlanCoordinator->ReleaseEnvironment(InCell);
		return { true, false, {} };
	}
	if (const FEnvironmentColumnAttempt* Existing = EnvironmentColumnStates.Find(InCell)) return Existing->Status;
	const FVoxelGenerationDependencyStatus Status = PlanCoordinator->EnsureEnvironment(InCell);
	if (!Status.bReady) return Status;
	if (EnvironmentColumnStates.Num() >= 32) return {};
	const auto Retention = Generator->RetainHydrologyForPlanning(InCell, 0);
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::ProjectBackground;
	Request.WorkClass = EVoxelWorkClass::Interactive;
	Request.Stamp.OwnerId = EnvironmentTaskOwnerId;
	Request.Stamp.OwnerIncarnation = EnvironmentTaskIncarnation;
	Request.Stamp.WorldEpoch = Runtime.Epoch();
	Request.Stamp.Section = FIntVector(InCell.X, InCell.Y, 0);
	check(NextEnvironmentTaskSerial != MAX_uint64);
	Request.Stamp.Token = ++NextEnvironmentTaskSerial;
	Request.Stamp.Revision = 1;
	Request.ReservedBytes = 4096;
	Request.Execute = [Pipeline = Generator, InCell, Retention](const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		FVoxelEnvironmentSample Sample;
		Result.bSuccess = !Cancel.Load() && Pipeline->SampleEnvironment(
			InCell.X, InCell.Y, Sample, Result.Error, &Cancel);
		Result.bCanceled = Cancel.Load();
		return Result;
	};
	Request.Apply = [this, InCell](FVoxelTaskResult&& Result)
	{
		const auto* Current = EnvironmentColumnStates.Find(InCell);
		if (!Current || Current->Stamp != Result.Stamp || Result.Stamp.WorldEpoch != Runtime.Epoch() ||
			Result.Stamp.OwnerIncarnation != EnvironmentTaskIncarnation) return;
		EnvironmentColumnStates.Remove(InCell);
		if (!Result.bSuccess && !Result.bCanceled && !Result.Error.StartsWith(TEXT("DependencyNotReady")))
		{
			EnvironmentColumnStates.Add(InCell, { Result.Stamp, { false, true, Result.Error } });
			UE_LOG(LogTemp, Error, TEXT("Environment column %s failed: %s"), *InCell.ToString(), *Result.Error);
		}
	};
	const auto Stamp = Request.Stamp;
	if (Scheduler.Enqueue(MoveTemp(Request))) EnvironmentColumnStates.Add(InCell, { Stamp, {} });
	return {};
}

FVoxelGenerationDependencyStatus FVoxelEmergeManager::EnsureEnvironmentBounds(
	const FVoxelGenerationBounds& InBounds, const EVoxelWorkClass InWorkClass,
	const int32 InSourcePriority, const double InDistanceScore, const double InForwardScore)
{
	return PlanCoordinator->EnsureEnvironmentBounds(InBounds, InWorkClass,
		InSourcePriority, InDistanceScore, InForwardScore);
}

void FVoxelEmergeManager::Reset()
{
	check(EnvironmentTaskIncarnation != MAX_uint64);
	const uint64 PreviousIncarnation = EnvironmentTaskIncarnation++;
	Scheduler.CancelMatching([Owner = EnvironmentTaskOwnerId, PreviousIncarnation](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::ProjectBackground && Stamp.OwnerId == Owner && Stamp.OwnerIncarnation == PreviousIncarnation;
	});
	for (const auto& Pair : ExternalFactConsumers) Generator->ReleaseOverlayConsumer(Pair.Key);
	for (const auto& Pair : WaitingPlanSections) Generator->ReleaseOverlayConsumer(Pair.Value.Consumer);
	ExternalFactConsumers.Reset();
	verify(AdmissionSelection.Reset());
	AdmissionAges.Reset();
	AdmissionCostEstimateMicroseconds = 25.0;
	CurrentDemand.Reset();
	for (const auto& Pair : ExternalDataConsumers)
	{
		verify(Runtime.ReleaseSectionPin(Pair.Value.Section, Pair.Value.Pin.Resource));
	}
	ExternalDataDemands.Empty();
	ExternalDataOrder.Empty();
	ExternalDataConsumers.Empty();
	ExternalDataStorageLease.Reset();
	ExternalDataCapacity = ExternalDataConsumerCapacity = ExternalDataCursor = 0;
	CriticalAdmissionKeys.Reset();
	ReadyCriticalSections.Empty();
	QueuedCriticalSections.Reset();
	CriticalFallbackCursor = 0;
	WaitingPlanSections.Reset();
	EnvironmentColumnStates.Reset();
	PlanCoordinator->Reset();
	RemoteChangeStates.Reset();
	OrderedKeys.Reset();
	for (auto& Lane : AdmissionLanes) Lane.Reset();

	CurrentInterestRevision = 0;
	CurrentPriorityRevision = 0;
	NextAdmissionIndices[0] = 0;
	NextAdmissionIndices[1] = 0;
	LastAdmissionDiagnosticTime = 0.0;
	PlanPendingAttempts = 0;
	PlanReadyAttempts = 0;
	OverlayPendingAttempts = 0;
	OverlayReadyAttempts = 0;
	EnqueueAttempts = 0;
	EnqueuedTasks = 0;
}

void FVoxelEmergeManager::SetRemoteOverlayResolver(TFunction<bool(const FIntVector&, uint64&,
	TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>&)> InResolver)
{
	RemoteOverlayResolver = MoveTemp(InResolver);
}

FVoxelGenerationDependencyStatus FVoxelEmergeManager::EnsureGenerationFacts(
	const FVoxelGenerationConsumerKey& InConsumer, const FVoxelGenerationBounds& InBounds,
	const FVoxelTerrainServicePriority& InPriority)
{
	check(IsInGameThread());
	if (!InConsumer.IsValid() || InConsumer.WorldEpoch != Runtime.Epoch() || !InBounds.IsValid())
	{
		return {false, true, TEXT("外部事实消费者缺少完整身份或范围")};
	}
	FExternalFactConsumer* Consumer = ExternalFactConsumers.Find(InConsumer);
	if (Consumer && (Consumer->Bounds.Min != InBounds.Min || Consumer->Bounds.Max != InBounds.Max))
	{
		return {false, true, TEXT("同一事实Consumer不能改变捕获范围")};
	}
	if (!Consumer)
	{
		FExternalFactConsumer Candidate;
		Candidate.Bounds = InBounds;
		FString Error;
		if (!PlanCoordinator->GatherForBounds(InBounds, Candidate.Dependencies, Error)) return {false, true, MoveTemp(Error)};
		Candidate.Retention = PlanCoordinator->RetainDependencies(Candidate.Dependencies,
			FIntPoint(int32((int64(InBounds.Min.X) + InBounds.Max.X) / 2), int32((int64(InBounds.Min.Y) + InBounds.Max.Y) / 2)));
		Consumer = &ExternalFactConsumers.Add(InConsumer, MoveTemp(Candidate));
	}
	const EVoxelWorkClass WorkClass = InPriority.Band <= EVoxelTerrainServiceBand::P1 ? EVoxelWorkClass::Critical : EVoxelWorkClass::Visible;
	const auto Natural = PlanCoordinator->Ensure(Consumer->Dependencies, WorkClass, InPriority, InConsumer);
	const auto Overlay = Generator->EnsureOverlayConsumerReady(InConsumer, InBounds, WorkClass, InPriority);
	if (Natural.bFailed) return Natural;
	if (Overlay.bFailed) return {false, true, Overlay.Error};
	return {Natural.bReady && Overlay.bReady, false, {}};
}

void FVoxelEmergeManager::ReleaseGenerationFacts(const FVoxelGenerationConsumerKey& InConsumer)
{
	check(IsInGameThread());
	PlanCoordinator->ReleaseConsumer(InConsumer);
	Generator->ReleaseOverlayConsumer(InConsumer);
	ExternalFactConsumers.Remove(InConsumer);
}

void FVoxelEmergeManager::SetExternalConsumerWakeCallback(TFunction<void(const FVoxelGenerationConsumerKey&)> InCallback)
{
	check(IsInGameThread());
	ExternalConsumerWakeCallback = MoveTemp(InCallback);
}

bool FVoxelEmergeManager::ReserveExternalDataStorage(const int32 InSectionCount, const int32 InConsumerCount)
{
	if (InSectionCount <= ExternalDataCapacity && InConsumerCount <= ExternalDataConsumerCapacity && ExternalDataStorageLease) return true;
	const int64 Rows = FMath::Max<int64>(16, InSectionCount > ExternalDataCapacity ?
		FMath::Max<int64>(InSectionCount, int64(ExternalDataCapacity) * 2) : ExternalDataCapacity);
	const int64 ConsumerRows = FMath::Max<int64>(16, InConsumerCount > ExternalDataConsumerCapacity ?
		FMath::Max<int64>(InConsumerCount, int64(ExternalDataConsumerCapacity) * 2) : ExternalDataConsumerCapacity);
	if (Rows > MAX_int32 || ConsumerRows > MAX_int32) return false;
	const uint64 NewBytes = VoxelBoundedAllocation::MapBytes<FIntVector, FExternalDataDemand>(int32(Rows)) +
		VoxelBoundedAllocation::SparseArrayBytes<FIntVector>(int32(Rows)) +
		VoxelBoundedAllocation::MapBytes<FGuid, FExternalDataConsumer>(int32(ConsumerRows));
	const uint64 Peak = NewBytes + ExternalDataDemands.GetAllocatedSize() + ExternalDataOrder.GetAllocatedSize() +
		ExternalDataConsumers.GetAllocatedSize();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(Runtime.Epoch(), GenerationOwnerId, 1, {}, 1);
	if ((!ExternalDataStorageLease && !Scheduler.GetTerrainResources()->Acquire(Consumer,
		EVoxelTerrainResourcePool::Input, Peak, ExternalDataStorageLease)) || !ExternalDataStorageLease->Resize(Peak)) return false;
	ExternalDataDemands.Reserve(int32(Rows));
	ExternalDataOrder.Reserve(int32(Rows));
	ExternalDataConsumers.Reserve(int32(ConsumerRows));
	ExternalDataCapacity = int32(Rows);
	ExternalDataConsumerCapacity = int32(ConsumerRows);
	const uint64 Actual = ExternalDataDemands.GetAllocatedSize() + ExternalDataOrder.GetAllocatedSize() + ExternalDataConsumers.GetAllocatedSize();
	check(Actual <= NewBytes);
	verify(ExternalDataStorageLease->Resize(Actual));
	return true;
}

bool FVoxelEmergeManager::AcquireDataDemand(const FIntVector& InSection, FVoxelRuntimeSectionPin& OutPin)
{
	check(IsInGameThread());
	auto* Existing = ExternalDataDemands.Find(InSection);
	if (Existing && Existing->References == MAX_int32) return false;
	if (ExternalDataDemands.Num() == MAX_int32 || ExternalDataConsumers.Num() == MAX_int32 ||
		!ReserveExternalDataStorage(ExternalDataDemands.Num() + (Existing ? 0 : 1), ExternalDataConsumers.Num() + 1)) return false;
	// Reserve 可搬移聚合表；其字段只能在新的地址上读取。
	Existing = ExternalDataDemands.Find(InSection);
	auto* Section = Runtime.FindOrAllocate(InSection, GFrameCounter);
	if (!Section) return false;
	const auto Identity = Section->GetResourceIdentity();
	if ((Existing && Existing->Identity != Identity) || !Runtime.TryPinSection(InSection, Identity)) return false;
	if (!Existing)
	{
		FExternalDataDemand Record;
		Record.Identity = Identity;
		Record.Demand.bData = true;
		Record.Demand.bExact = true;
		Record.Demand.ServicePriority.Band = EVoxelTerrainServiceBand::P2;
		Record.Demand.ServicePriority.SourceStableKey = GenerationOwnerId;
		Record.OrderIndex = ExternalDataOrder.Add(InSection);
		Existing = &ExternalDataDemands.Add(InSection, MoveTemp(Record));
	}
	++Existing->References;
	OutPin = {Identity, FGuid::NewGuid()};
	ExternalDataConsumers.Add(OutPin.ConsumerId, {InSection, OutPin});
	return true;
}

bool FVoxelEmergeManager::ReleaseDataDemand(const FIntVector& InSection, const FVoxelRuntimeSectionPin& InPin)
{
	check(IsInGameThread());
	auto* Demand = ExternalDataDemands.Find(InSection);
	const auto* Consumer = ExternalDataConsumers.Find(InPin.ConsumerId);
	const auto* Section = Runtime.FindSection(InSection);
	if (!Consumer || Consumer->Section != InSection || Consumer->Pin != InPin || !Demand ||
		Demand->Identity != InPin.Resource || Demand->References <= 0 || !Section ||
		Section->GetResourceIdentity() != InPin.Resource) return false;
	ExternalDataConsumers.Remove(InPin.ConsumerId);
	const bool bLast = --Demand->References == 0;
	if (bLast)
	{
		ExternalDataOrder.RemoveAt(Demand->OrderIndex);
		ExternalDataDemands.Remove(InSection);
		if (!CurrentDemand.Contains(InSection))
		{
			if (const auto* Waiting = WaitingPlanSections.Find(InSection))
			{
				PlanCoordinator->ReleaseConsumer(Waiting->Consumer);
				Generator->ReleaseOverlayConsumer(Waiting->Consumer);
				WaitingPlanSections.Remove(InSection);
			}
			AdmissionAges.Remove(InSection);
			Scheduler.CancelMatching([this, InSection](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
			{
				return (Kind == EVoxelTaskKind::GenerateExactBase || Kind == EVoxelTaskKind::DecodeOverlay) &&
					Stamp.OwnerId == GenerationOwnerId && Stamp.WorldEpoch == Runtime.Epoch() && Stamp.Section == InSection;
			});
		}
	}
	verify(Runtime.ReleaseSectionPin(InSection, InPin.Resource));
	return true;
}

const FVoxelExactDemand* FVoxelEmergeManager::FindDemand(const FIntVector& InSection) const
{
	const auto* Interest = CurrentDemand.Find(InSection);
	const auto* External = ExternalDataDemands.Find(InSection);
	if (External && (!Interest || FVoxelTerrainWorkSelection::HigherPriority(External->Demand.ServicePriority, Interest->ServicePriority)))
		return &External->Demand;
	return Interest;
}

EVoxelWorkClass FVoxelEmergeManager::ResolveWorkClass(const FVoxelExactDemand& InDemand)
{
	if (IsCriticalDemand(InDemand))
	{
		return EVoxelWorkClass::Critical;
	}

	return InDemand.bExact || InDemand.bCollision || InDemand.bSimulation ? EVoxelWorkClass::ExactData : EVoxelWorkClass::Visible;
}

void FVoxelEmergeManager::RebuildDemand(
	const FVoxelInterestSet& InInterest,
	const uint64 InInterestRevision)
{
	for (const auto& Pair : WaitingPlanSections)
	{
		const FVoxelExactDemand* Previous = CurrentDemand.Find(Pair.Key);
		const FVoxelExactDemand* Next = InInterest.Exact.Find(Pair.Key);
		if (const auto* External = ExternalDataDemands.Find(Pair.Key);
			External && (!Next || FVoxelTerrainWorkSelection::HigherPriority(External->Demand.ServicePriority, Next->ServicePriority))) Next = &External->Demand;
		if (Previous && Next &&
			(ResolveWorkClass(*Previous) != ResolveWorkClass(*Next) || Previous->Priority != Next->Priority || Previous->DistanceCells != Next->DistanceCells ||
				Previous->ForwardScore != Next->ForwardScore))
		{
				PlanCoordinator->Ensure(Pair.Value.Dependencies, ResolveWorkClass(*Next), Next->ServicePriority, Pair.Value.Consumer);
			const FIntVector Min = Pair.Key * 16;
			Generator->EnsureOverlayConsumerReady(Pair.Value.Consumer, {Min, Min + FIntVector(16)}, ResolveWorkClass(*Next), Next->ServicePriority);
		}
	}
	CurrentDemand = InInterest.Exact;
	CriticalAdmissionKeys.Reset();
	for (const FIntVector& Key : InInterest.ExactOrder)
	{
		if (IsCriticalDemand(InInterest.Exact.FindChecked(Key)))
		{
			CriticalAdmissionKeys.Add(Key);
			WakeCriticalSection(Key);
		}
	}
	CriticalFallbackCursor = CriticalAdmissionKeys.IsEmpty() ? 0 : CriticalFallbackCursor % CriticalAdmissionKeys.Num();
	OrderedKeys = InInterest.ExactOrder;
	for (int32 Lane = 0; Lane < 2; ++Lane) AdmissionLanes[Lane] = InInterest.ExactAdmissionLanes[Lane];
	for (auto Iterator = WaitingPlanSections.CreateIterator(); Iterator; ++Iterator)
	{
		if (!FindDemand(Iterator.Key()))
		{
			PlanCoordinator->ReleaseConsumer(Iterator.Value().Consumer);
			Generator->ReleaseOverlayConsumer(Iterator.Value().Consumer);
			Iterator.RemoveCurrent();
		}
	}
	Scheduler.UpdateServicePriorities([this](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp,
		EVoxelWorkClass& WorkClass, FVoxelTerrainServicePriority& Priority)
	{
		if (Kind != EVoxelTaskKind::GenerateExactBase && Kind != EVoxelTaskKind::DecodeOverlay) return;
		if (const FVoxelExactDemand* Demand = FindDemand(Stamp.Section))
		{
			WorkClass = ResolveWorkClass(*Demand);
			Priority = Demand->ServicePriority;
		}
	});

	CurrentInterestRevision =
		InInterestRevision;
	CurrentPriorityRevision = InInterest.PriorityRevision;

	NextAdmissionIndices[0] = 0;
	NextAdmissionIndices[1] = AdmissionLanes[1].IsEmpty() ? 0 : NextAdmissionIndices[1] % AdmissionLanes[1].Num();
}

bool FVoxelEmergeManager::RequestSection(
	const FIntVector& InKey,
	const FVoxelExactDemand& InDemand)
{
	FVoxelSection* Section =
		Runtime.FindOrAllocate(
			InKey,
			GFrameCounter);

	if (!Section)
	{
		return false;
	}
	if (Section->PendingPublish && Section->PendingPublish->Kind != EVoxelTaskKind::None)
	{
		if (Runtime.GetTerrainResources()->Snapshot().ReleaseVersion <= Section->PendingCapacityVersion) return false;
		auto Pending = MoveTemp(*Section->PendingPublish);
		*Section->PendingPublish = FVoxelTaskResult();
		OnTask(MoveTemp(Pending));
		return Section->PendingPublish->Kind == EVoxelTaskKind::None;
	}

	switch (Section->Status)
	{
	case EVoxelSectionStatus::Allocated:
		return RequestBase(
			*Section,
			InKey,
			InDemand);

	case EVoxelSectionStatus::BaseReady:
		return ResolveOverlay(
			*Section,
			InKey,
			InDemand);

	case EVoxelSectionStatus::DataReady:
		Section->bSimulationWanted =
			InDemand.bSimulation;
		break;

	case EVoxelSectionStatus::None:
	case EVoxelSectionStatus::Failed:
	case EVoxelSectionStatus::Unloading:
	default:
		break;
	}
	return false;
}

bool FVoxelEmergeManager::RequestBase(
	FVoxelSection& InSection,
	const FIntVector& InKey,
	const FVoxelExactDemand& InDemand)
{
	FVoxelTaskStamp TaskStamp;
	TaskStamp.WorldEpoch =
		InSection.Stamp.Epoch;
	TaskStamp.Token =
		InSection.Stamp.Token;
	TaskStamp.Section =
		InKey;
	TaskStamp.OwnerId = GenerationOwnerId;
	TaskStamp.OwnerIncarnation = InSection.Stamp.Token;

	if (Scheduler.Has(
		TaskStamp,
		EVoxelTaskKind::GenerateExactBase))
	{
		return false;
	}
	FWaitingPlanSection* Waiting = WaitingPlanSections.Find(InKey);
	if (Waiting && Waiting->Stamp != InSection.Stamp)
	{
		PlanCoordinator->ReleaseConsumer(Waiting->Consumer);
		Generator->ReleaseOverlayConsumer(Waiting->Consumer);
		WaitingPlanSections.Remove(InKey);
		Waiting = nullptr;
	}
	if (!Waiting)
	{
		TArray<FVoxelGenerationPlanDependency> Gathered;
		const FIntVector Min = InKey * 16;
		FString Error;
		if (!PlanCoordinator->GatherForBounds({ Min, Min + FIntVector(16) },
			Gathered, Error))
		{
			InSection.Status = EVoxelSectionStatus::Failed;
			InSection.Error = MoveTemp(Error);
			UE_LOG(LogTemp, Error, TEXT("Voxel plan preflight failed: section=%s error=%s"),
				*InKey.ToString(), *InSection.Error);
			return false;
		}
		Waiting = &WaitingPlanSections.Add(InKey);
		Waiting->Stamp = InSection.Stamp;
		checkf(NextGenerationRequestSerial != MAX_uint64, TEXT("Generation consumer serial exhausted"));
		Waiting->Consumer = FVoxelGenerationConsumerKey::Section(InSection.Stamp.Epoch, GenerationOwnerId,
			InSection.Stamp.Token, InKey, ++NextGenerationRequestSerial);
		Waiting->BlockedSinceSeconds = FPlatformTime::Seconds();
		Waiting->Dependencies = MoveTemp(Gathered);
		Waiting->Retention = PlanCoordinator->RetainDependencies(Waiting->Dependencies, FIntPoint(Min.X + 8, Min.Y + 8));
	}
	const double Now = FPlatformTime::Seconds();
	if (!IsCriticalDemand(InDemand) && Now < Waiting->RetryAfterSeconds)
	{
		return false;
	}
	const FVoxelGenerationDependencyStatus PlanStatus = PlanCoordinator->Ensure(
		Waiting->Dependencies,
		ResolveWorkClass(InDemand), InDemand.ServicePriority, Waiting->Consumer);
	if (PlanStatus.bFailed)
	{
		InSection.Status = EVoxelSectionStatus::Failed;
		InSection.Error = PlanStatus.Error;
		PlanCoordinator->ReleaseConsumer(Waiting->Consumer);
		Generator->ReleaseOverlayConsumer(Waiting->Consumer);
		WaitingPlanSections.Remove(InKey);
		UE_LOG(LogTemp, Error, TEXT("Voxel plan dependency failed: section=%s error=%s"),
			*InKey.ToString(), *InSection.Error);
		return false;
	}
	const FVoxelOverlayPreflightStatus OverlayStatus =
		Generator->EnsureOverlayConsumerReady(Waiting->Consumer, {InKey * 16, InKey * 16 + FIntVector(16)},
			ResolveWorkClass(InDemand), InDemand.ServicePriority);
	if (OverlayStatus.bFailed)
	{
		InSection.Status = EVoxelSectionStatus::Failed;
		InSection.Error = OverlayStatus.Error;
		PlanCoordinator->ReleaseConsumer(Waiting->Consumer);
		Generator->ReleaseOverlayConsumer(Waiting->Consumer);
		WaitingPlanSections.Remove(InKey);
		UE_LOG(LogTemp, Error,
			TEXT("Voxel overlay preflight failed: section=%s error=%s"),
			*InKey.ToString(), *InSection.Error);
		return false;
	}
	PlanPendingAttempts += !PlanStatus.bReady;
	PlanReadyAttempts += PlanStatus.bReady;
	OverlayPendingAttempts += !OverlayStatus.bReady;
	OverlayReadyAttempts += OverlayStatus.bReady;
	if (!PlanStatus.bReady || !OverlayStatus.bReady)
	{
		Waiting->RetryAfterSeconds = Now + (IsCriticalDemand(InDemand) ? 0.05 : 0.2);
		Waiting->Blocker = FString::Printf(TEXT("PlanReady=%d OverlayReady=%d %s"), PlanStatus.bReady, OverlayStatus.bReady, *OverlayStatus.Error);
		return false;
	}
	const auto CapturedRetention = Waiting->Retention;

	FVoxelTaskRequest Request;
	Request.Kind =
		EVoxelTaskKind::GenerateExactBase;

	Request.WorkClass = ResolveWorkClass(InDemand);
	Request.SourcePriority = InDemand.Priority;
	Request.ServiceBand = InDemand.ServicePriority.Band;
	Request.SourceStableKey = InDemand.ServicePriority.SourceStableKey;

	Request.Stamp =
		TaskStamp;

	Request.DistanceScore = InDemand.DistanceCells;
	Request.ForwardScore = InDemand.ForwardScore;

	const uint64 BaseBytes = VoxelBoundedAllocation::WorkerSharedBytes<TArray<FVoxelBlockState>>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelBlockState>(4096);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BaseLease;
	if (!Runtime.GetTerrainResources()->Acquire(Waiting->Consumer, EVoxelTerrainResourcePool::Staged, BaseBytes, BaseLease)) return false;
	Request.ReservedBytes = BaseBytes;

	const TSharedRef<
		const FVoxelGenerationPipeline,
		ESPMode::ThreadSafe> CapturedGenerator =
			Generator;

	Request.Execute =
		[CapturedGenerator, CapturedRetention, InKey, BaseLease](
			const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;
			Result.OutputResourceLease = BaseLease;

			Result.bSuccess =
				CapturedGenerator->GenerateSection(
					InKey,
					Result.BaseBlocks,
					Result.Error,
					&InCancel);

			Result.bCanceled =
				InCancel.Load();

			return Result;
		};

	++EnqueueAttempts;
	const bool bEnqueued = Scheduler.Enqueue(MoveTemp(Request));
	EnqueuedTasks += bEnqueued ? 1 : 0;
	if (bEnqueued)
	{
		// 租约随任务捕获，覆盖排队与执行；撤销需求或任务完成后自动释放。
		PlanCoordinator->ReleaseConsumer(Waiting->Consumer);
		// 项目快照保持到此尝试 Apply，覆盖后台构建期间的缓存淘汰。
	}
	return bEnqueued;
}

bool FVoxelEmergeManager::ResolveOverlay(
	FVoxelSection& InSection,
	const FIntVector& InKey,
	const FVoxelExactDemand& InDemand)
{
	if (InSection.Status !=
		EVoxelSectionStatus::BaseReady)
	{
		return false;
	}

	bool bKnownNatural = false;
	if (!Runtime.IsServer())
	{
		uint64 Revision = 0;
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Overlay;
		if (!RemoteOverlayResolver || !RemoteOverlayResolver(InKey, Revision, Overlay)) return false;
		FString Error;
		const TMap<int32, FVoxelBlockState> EmptyBlocks;
		const TMap<int32, FVoxelBlockEntityState> EmptyEntities;
		return Runtime.PublishFinal(InKey, Revision, Overlay ? Overlay->Blocks : EmptyBlocks,
			Overlay ? Overlay->Entities : EmptyEntities, Error);
	}

	// 已提交 Journal 也包含恢复自然值的 Tombstone；不能从稀疏磁盘索引推导版本为零。
	if (const auto Overlay = Runtime.FindCommittedOverlay(InKey))
	{
		FString Error;
		return Runtime.PublishFinal(InKey, Overlay->Revision, Overlay->Blocks, Overlay->Entities, Error);
	}
	if (!RegionStore.IsDirectoryReady()) return false;

	if (Runtime.IsServer())
	{
		bKnownNatural =
			!Runtime.GetChangeIndex().
				IsModified(InKey);
	}

	if (bKnownNatural)
	{
		FString Error;

		return Runtime.PublishFinal(
			InKey,
			0,
			{},
			{},
			Error);
	}

	if (!Runtime.IsServer())
	{
		return false;
	}

	FVoxelTaskRequest Request;
	Request.Kind =
		EVoxelTaskKind::DecodeOverlay;

	Request.WorkClass = ResolveWorkClass(InDemand);
	Request.SourcePriority = InDemand.Priority;
	Request.ServiceBand = InDemand.ServicePriority.Band;
	Request.SourceStableKey = InDemand.ServicePriority.SourceStableKey;

	Request.DistanceScore = InDemand.DistanceCells;
	Request.ForwardScore = InDemand.ForwardScore;

	Request.Stamp.WorldEpoch =
		InSection.Stamp.Epoch;

	Request.Stamp.Token =
		InSection.Stamp.Token;

	Request.Stamp.Section =
		InKey;
	Request.Stamp.OwnerId = GenerationOwnerId;
	Request.Stamp.OwnerIncarnation = InSection.Stamp.Token;

	if (Scheduler.Has(
		Request.Stamp,
		Request.Kind))
	{
		return false;
	}

	const auto Resources = Runtime.GetTerrainResources();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(Runtime.Epoch(), GenerationOwnerId,
		InSection.Stamp.Token, InKey, ++NextGenerationRequestSerial);
	const uint64 InputBytes = VoxelBoundedAllocation::WorkerSharedBytes<FExactOverlayDecodeWork>() +
		VoxelBoundedAllocation::ArrayBytes<TCHAR>(RegionStore.GetSourceDirectory().Len() + 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, InputBytes, InputLease)) return false;
	auto Work = VoxelBoundedAllocation::MakeWorkerShared<FExactOverlayDecodeWork>(InputLease);
	Work->Read = RegionStore.CaptureRead(InKey);
	Work->Manifest = Manifest;
	Work->Registry = Registry;
	Work->Resources = Resources;
	Work->Consumer = Consumer;
	Request.InputBytes = InputBytes;
	Request.ReservedBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelPersistentSection>();
	Request.bResumable = true;
	Request.GetBlockReason = [Work]
	{
		return Work->bWaitingCapacity.Load() && Work->Resources->Snapshot().ReleaseVersion <= Work->CapacityVersion.Load() ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	};
	Request.Execute = [Work](const TAtomic<bool>& InCancel) { return Work->Advance(InCancel); };

	return Scheduler.Enqueue(MoveTemp(Request));
}

uint64 FVoxelEmergeManager::GetWaitingPlanConsumers(const EVoxelGenerationPlanKind InKind) const
{
	return PlanCoordinator->GetWaitingConsumers(InKind);
}

bool FVoxelEmergeManager::IsCriticalDemand(const FVoxelExactDemand& InDemand)
{
	return InDemand.bWarmupData || InDemand.bWarmupCollision || InDemand.bMovementCriticalData || InDemand.bMovementCriticalFine ||
		   InDemand.bMovementCriticalCollision;
}

FVoxelEmergeManager::~FVoxelEmergeManager()
{
	Generator->SetOverlayPlanningServices({});
	Generator->SetTypedOverlayConsumerWakeCallback({});
	Generator->SetOverlayServicePriorityDonation({});
	Generator->SetOverlayConsumerWakeCallback({});
	Reset();
}

void FVoxelEmergeManager::OnGenerationFactsReady(const FVoxelGenerationConsumerKey& InConsumer)
{
	if (InConsumer.WorldEpoch != Runtime.Epoch()) return;
	if (InConsumer.Kind == EVoxelGenerationConsumerKind::WorldFact)
	{
		Generator->NotifyOverlayPlanningDependencyReady(InConsumer);
		return;
	}
	if (ExternalFactConsumers.Contains(InConsumer))
	{
		if (ExternalConsumerWakeCallback) ExternalConsumerWakeCallback(InConsumer);
		return;
	}
	if (InConsumer.Kind != EVoxelGenerationConsumerKind::F0Section || InConsumer.OwnerId != GenerationOwnerId) return;
	FWaitingPlanSection* Waiting = WaitingPlanSections.Find(InConsumer.Min);
	if (Waiting && Waiting->Consumer == InConsumer && Runtime.IsCurrentStamp(InConsumer.Min, Waiting->Stamp))
	{
		Waiting->RetryAfterSeconds = 0.0;
		WakeCriticalSection(InConsumer.Min);
	}
}

void FVoxelEmergeManager::WakeCriticalSection(const FIntVector& InKey)
{
	const FVoxelExactDemand* Demand = CurrentDemand.Find(InKey);
	if (!Demand || !IsCriticalDemand(*Demand))
	{
		return;
	}
	FWaitingPlanSection* Waiting = WaitingPlanSections.Find(InKey);
	if (Waiting && !Runtime.IsCurrentStamp(InKey, Waiting->Stamp))
	{
		return;
	}
	if (Waiting)
	{
		Waiting->RetryAfterSeconds = 0.0;
	}
	if (!QueuedCriticalSections.Contains(InKey))
	{
		QueuedCriticalSections.Add(InKey);
		ReadyCriticalSections.Enqueue(InKey);
	}
}
