#include "Voxel/Rendering/VoxelViewManager.h"

#include "Async/Async.h"

#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Network/VoxelOverlayReplica.h"
#include "Voxel/Runtime/VoxelFineOverlayRead.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/VoxelModule.h"

namespace
{
	TSharedPtr<const void, ESPMode::ThreadSafe> PinFineIndex(UVoxelModule& InModule,
		const TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InConsumer, const FVoxelFineShardKey& InKey, const uint64 InMetadataBytes)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata;
		if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Resident, InMetadataBytes, Metadata)) return nullptr;
		InModule.GetRuntime()->GetChangeHierarchy().RegisterFineShard(InKey);
		const TWeakObjectPtr<UVoxelModule> WeakModule(&InModule);
		const uint64 Epoch = InModule.GetWorldEpoch();
		return TSharedPtr<const void, ESPMode::ThreadSafe>(new uint8(0),
			[WeakModule, Epoch, InKey, Metadata](const void* InPointer)
			{
				const auto Release = [WeakModule, Epoch, InKey, InPointer, Metadata]
				{
					if (UVoxelModule* Owner = WeakModule.Get(); Owner && Owner->GetWorldEpoch() == Epoch && Owner->GetRuntime())
					{
						Owner->GetRuntime()->GetChangeHierarchy().ReleaseFineShard(InKey);
					}
					delete static_cast<const uint8*>(InPointer);
				};
				if (IsInGameThread()) Release();
				else AsyncTask(ENamedThreads::GameThread, Release);
			});
	}

	struct FF0FineWorker
	{
		TSharedPtr<const FVoxelFineShardSamples, ESPMode::ThreadSafe> Samples;
		TUniquePtr<FVoxelFineShardMeshing> Mesher;
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
		TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
		TSharedRef<const FVoxelShapeRegistry, ESPMode::ThreadSafe> Shapes;
		FVoxelGenerationConsumerKey Consumer;
		FVoxelFineStreamingPolicy Policy;
		TAtomic<bool> bWaitingCapacity{false};
		TAtomic<uint64> CapacityVersion{0};
		uint64 BeforeCapacityBuild = 0;

		FF0FineWorker(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
			TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
			TSharedRef<const FVoxelShapeRegistry, ESPMode::ThreadSafe> InShapes)
			: Resources(InResources), Registry(InRegistry), Shapes(InShapes)
		{
		}

		FVoxelTaskResult Advance(const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;
			BeforeCapacityBuild = Resources->ReleaseVersionExcludingOwner(Consumer);
			if (!Mesher)
			{
				auto Candidate = MakeUnique<FVoxelFineShardMeshing>(Samples.ToSharedRef(), Registry, Shapes, Resources, Consumer);
				if (!Candidate->Initialize(uint64(Policy.MaximumMeshPieceMegabytes) * 1024 * 1024,
					uint64(Policy.MaximumShardGeometryMegabytes) * 1024 * 1024, Result.Error))
				{
					if (Result.Error.StartsWith(TEXT("CapacityBlocked:")))
					{
						CapacityVersion.Store(BeforeCapacityBuild);
						bWaitingCapacity.Store(true);
						Result.bSuccess = Result.bContinue = true;
						Result.Error.Reset();
					}
					return Result;
				}
				Mesher = MoveTemp(Candidate);
				bWaitingCapacity.Store(false);
			}
			if (bWaitingCapacity.Load())
			{
				if (!Mesher->ResumeCapacity(Resources->Snapshot().ReleaseVersion, Result.Error))
				{
					CapacityVersion.Store(BeforeCapacityBuild);
					Result.bSuccess = Result.bContinue = true;
					Result.Error.Reset();
					return Result;
				}
				bWaitingCapacity.Store(false);
			}
			const auto Progress = Mesher->Advance(InCancel, Result.Error);
			if (Progress == EVoxelFineBuildProgress::CapacityBlocked && Mesher->IsCapacityResumable())
			{
				CapacityVersion.Store(BeforeCapacityBuild);
				bWaitingCapacity.Store(true);
				Result.bSuccess = Result.bContinue = true;
				Result.Error.Reset();
			}
			else if (Progress == EVoxelFineBuildProgress::Complete)
			{
				Result.CustomPayload = Mesher->TakeResult();
				Result.bSuccess = Result.CustomPayload.IsValid();
				Samples.Reset();
			}
			else if (Progress == EVoxelFineBuildProgress::Working) Result.bSuccess = Result.bContinue = true;
			else Result.bCanceled = Progress == EVoxelFineBuildProgress::Canceled;
			return Result;
		}
	};

	struct FFineShardWorker
	{
		TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> SamplesReady;
		TSharedPtr<const void, ESPMode::ThreadSafe> RegistrationLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CaptureLease;
		FVoxelFineOverlayRead Reader;
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> FactResources;
		TAtomic<bool> bWaitingFacts{false};
		TAtomic<uint64> FactCapacityVersion{0};
		uint64 BeforeFactBuild = 0;
		TSharedPtr<const FVoxelFineOverlayReadResult, ESPMode::ThreadSafe> Overlay;
		TUniquePtr<FVoxelFineShardSampler> Sampler;
		TUniquePtr<FVoxelFineShardMeshing> Mesher;
		TSharedPtr<const FVoxelFineShardSamples, ESPMode::ThreadSafe> Samples;
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
		TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
		TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
		TSharedRef<const FVoxelShapeRegistry, ESPMode::ThreadSafe> Shapes;
		FVoxelGenerationConsumerKey Consumer;
		FVoxelFineBuildAttempt Attempt;
		FVoxelFineStreamingPolicy Policy;
		uint64 RecipeHash = 0;
		int32 WorldMinZ = 0;
		int32 WorldMaxZ = 0;
		TAtomic<bool> bWaitingCapacity{false};
		TAtomic<uint64> CapacityVersion{0};
		uint64 BeforeCapacityBuild = 0;

		FFineShardWorker(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
			TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
			TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
			TSharedRef<const FVoxelShapeRegistry, ESPMode::ThreadSafe> InShapes)
			: Resources(InResources), Generator(InGenerator), Registry(InRegistry), Shapes(InShapes)
		{
		}

		FVoxelTaskResult WaitFacts()
		{
			FactCapacityVersion.Store(BeforeFactBuild);
			bWaitingFacts.Store(true);
			FVoxelTaskResult Result;
			Result.bSuccess = Result.bContinue = true;
			return Result;
		}

		FVoxelTaskResult WaitCapacity()
		{
			CapacityVersion.Store(BeforeCapacityBuild);
			bWaitingCapacity.Store(true);
			FVoxelTaskResult Result;
			Result.bSuccess = Result.bContinue = true;
			return Result;
		}

		FVoxelTaskResult Advance(const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;
			if (InCancel.Load()) { Result.bCanceled = true; return Result; }
			BeforeCapacityBuild = Resources->ReleaseVersionExcludingOwner(Consumer);
			if (bWaitingFacts.Load())
			{
				if (FactResources->ReleaseVersionExcludingOwner(Consumer) <= FactCapacityVersion.Load())
				{
					Result.bSuccess = Result.bContinue = true;
					return Result;
				}
				bWaitingFacts.Store(false);
			}
			BeforeFactBuild = FactResources->ReleaseVersionExcludingOwner(Consumer);
			if (bWaitingCapacity.Load())
			{
				if (BeforeCapacityBuild <= CapacityVersion.Load())
				{
					Result.bSuccess = Result.bContinue = true;
					return Result;
				}
				if (Mesher && !Mesher->ResumeCapacity(Resources->Snapshot().ReleaseVersion, Result.Error)) return WaitCapacity();
				bWaitingCapacity.Store(false);
			}
			if (!Overlay)
			{
				bool bComplete = false;
				if (!Reader.Advance(1, InCancel, bComplete, Result.Error))
				{
					if (!InCancel.Load() && Result.Error.StartsWith(TEXT("CapacityBlocked"))) return WaitFacts();
					return Result;
				}
				if (Result.Error.StartsWith(TEXT("CapacityBlocked:"))) return WaitFacts();
				if (!bComplete) { Result.bSuccess = Result.bContinue = true; return Result; }
				auto Read = MakeShared<FVoxelFineOverlayReadResult, ESPMode::ThreadSafe>();
				if (!Reader.TakeResult(*Read)) { Result.Error = TEXT("Fine Shard无法取得完整Overlay"); return Result; }
				Overlay = MoveTemp(Read);
			}
			if (!Mesher)
			{
				if (!Samples && !Sampler)
				{
					auto Candidate = MakeUnique<FVoxelFineShardSampler>(Generator, Overlay->Snapshot.ToSharedRef(), Resources,
						Consumer, RegistrationLease, Overlay->VersionProof);
					if (!Candidate->Initialize(Attempt, RecipeHash, WorldMinZ, WorldMaxZ, Result.Error))
					{
						if (Result.Error.StartsWith(TEXT("CapacityBlocked"))) return WaitCapacity();
						return Result;
					}
					Sampler = MoveTemp(Candidate);
				}
				if (!Samples)
				{
					const auto Progress = Sampler->Advance(1, InCancel, Result.Error);
					if (Progress == EVoxelFineBuildProgress::CapacityBlocked) return WaitFacts();
					if (Progress == EVoxelFineBuildProgress::Working) { Result.bSuccess = Result.bContinue = true; return Result; }
					if (Progress != EVoxelFineBuildProgress::Complete) return Result;
					Samples = Sampler->TakeResult();
					Sampler.Reset();
				}
				if (!Samples) { Result.Error = TEXT("Fine Shard采样结果不完整"); return Result; }
				if (SamplesReady) SamplesReady->Store(true);
				auto Candidate = MakeUnique<FVoxelFineShardMeshing>(Samples.ToSharedRef(), Registry, Shapes, Resources, Consumer);
				if (!Candidate->Initialize(uint64(Policy.MaximumMeshPieceMegabytes) * 1024 * 1024,
					uint64(Policy.MaximumShardGeometryMegabytes) * 1024 * 1024, Result.Error))
				{
					if (Result.Error.StartsWith(TEXT("CapacityBlocked"))) return WaitCapacity();
					return Result;
				}
				Mesher = MoveTemp(Candidate);
				Samples.Reset();
			}
			const auto Progress = Mesher->Advance(InCancel, Result.Error);
			if (Progress == EVoxelFineBuildProgress::CapacityBlocked)
			{
				if (Mesher->IsCapacityResumable()) return WaitCapacity();
				return Result;
			}
			if (Progress == EVoxelFineBuildProgress::Working) { Result.bSuccess = Result.bContinue = true; return Result; }
			if (Progress != EVoxelFineBuildProgress::Complete) return Result;
			Result.CustomPayload = Mesher->TakeResult();
			Mesher.Reset();
			Result.OverlayReadResult = Overlay;
			Result.InputResourceLease = CaptureLease;
			Result.bSuccess = Result.CustomPayload.IsValid();
			return Result;
		}
	};

	bool CaptureBounds(const FVoxelFineShardKey& InKey, FVoxelGenerationBounds& OutBounds)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const int64 Min = int64(InKey.Min[Axis]) - 16;
			const int64 Max = int64(InKey.Min[Axis]) + InKey.SideCells + 16;
			if (Min < MIN_int32 || Max > MAX_int32) return false;
			OutBounds.Min[Axis] = int32(Min);
			OutBounds.Max[Axis] = int32(Max);
		}
		return true;
	}
}

void FVoxelViewManager::ProcessFineShardAdmissions()
{
	const auto& Interest = GetViewInterest();
	if (!Interest.PublicationInputs || Interest.PublicationInputs->FineShards.IsEmpty()) return;
	const auto& Keys = Interest.PublicationInputs->FineShards;
	const double Deadline = TerrainDeadline(0.5);
	TArray<FVoxelTerrainServiceCandidate> Candidates;
	TArray<FVoxelFineShardKey> Captured;
	for (int32 Scans = 0; Scans < 64 && FPlatformTime::Seconds() < Deadline; ++Scans)
	{
		if (FineShardAdmissionCursor >= Keys.Num()) FineShardAdmissionCursor = 0;
		const FVoxelFineShardKey Key = Keys[FineShardAdmissionCursor++];
		const auto* Demand = Interest.FineShardDemands.Find(Key);
		if (!Demand) continue;
		FFineShardSlot* Slot = FineShardSlots.Find(Key);
		if (Slot && (Slot->bSubmitted || Slot->Build.IsMeshReady() || !Slot->Build.GetFailure().IsEmpty())) continue;
		FVoxelTerrainServiceCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Index = Captured.Add(Key);
		Candidate.Priority = Demand->ServicePriority;
		Candidate.FirstQueuedSeconds = Slot ? Slot->FirstQueuedSeconds : Demand->FirstQueuedSeconds;
		Candidate.Sequence = Slot ? Slot->QueueSequence : Demand->QueueSequence;
		Candidate.EstimatedMicroseconds = FineShardAdmissionCostMicroseconds;
		Candidate.bRunnable = true;
	}
	FVoxelTerrainServiceCapacity Capacity;
	Capacity.bAllowOverdueOpportunity = Module.GetTerrainFrameContext().IsOverdueOpportunityAvailable();
	Capacity.MaximumWaitSeconds = Module.GetViewSettings().Fine.MaximumBackgroundWaitSeconds;
	while (FPlatformTime::Seconds() < Deadline)
	{
		FVoxelTerrainServiceCharge Charge;
		const int32 Chosen = FineShardAdmissionSelection.Select(Candidates, Capacity, FPlatformTime::Seconds(), Charge);
		if (Chosen == INDEX_NONE) break;
		if (Charge.bOverdueOpportunity) Module.GetTerrainFrameContext().ConsumeOverdueOpportunity();
		const double Started = FPlatformTime::Seconds();
		RequestFineShard(Captured[Chosen]);
		const double Actual = (FPlatformTime::Seconds() - Started) * 1000000.0;
		FineShardAdmissionSelection.Settle(Charge.Serial, Actual);
		FineShardAdmissionCostMicroseconds = FMath::Lerp(FineShardAdmissionCostMicroseconds, FMath::Max(1.0, Actual), 0.15);
		Candidates[Chosen].bRunnable = false;
	}
}

bool FVoxelViewManager::RequestFineShard(const FVoxelFineShardKey& InKey)
{
	const auto* Demand = GetViewInterest().FineShardDemands.Find(InKey);
	const auto Generator = Module.GetGenerator();
	const auto Registry = Module.GetRegistry();
	const auto Shapes = Module.GetShapes();
	if (!Demand || !Generator || !Registry || !Shapes) return false;
	FFineShardSlot* Slot = FineShardSlots.Find(InKey);
	if (!Slot)
	{
		FVoxelFineMeshInput Input;
		Input.WorldEpoch = WorldEpoch;
		check(NextFineShardIncarnation < MAX_uint64);
		Input.Incarnation = ++NextFineShardIncarnation;
		Input.GeneratorIdentity = Module.GetManifest().BaseSampleHash;
		Input.RegistryRevision = Registry->Hash;
		Input.ShapeRevision = Shapes->GetContentHash();
		Input.Shard = InKey;
		Input.MeshRevision = 1;
		FFineShardSlot Created;
		const FVoxelGenerationConsumerKey RegistrationConsumer{EVoxelGenerationConsumerKind::FineShard, WorldEpoch,
			FineShardOwnerId, Input.Incarnation, EVoxelGenerationSpatialKind::Bounds, InKey.Min, InKey.Bounds().Max, 1};
		Created.RegistrationLease = PinFineIndex(Module, Scheduler.GetTerrainResources(), RegistrationConsumer, InKey,
			sizeof(FFineShardSlot) + 512);
		if (!Created.RegistrationLease) return false;
		Input.DataRevision = Input.OverlayRevision = Module.GetRuntime()->GetChangeHierarchy().GetFineShardRevision(InKey);
		FString Error;
		if (!Created.Build.Initialize(Input, Error)) return false;
		Created.FirstQueuedSeconds = Demand->FirstQueuedSeconds;
		Created.QueueSequence = Demand->QueueSequence;
		Created.OrderIndex = FineShardSlotOrder.Add(InKey);
		Slot = &FineShardSlots.Add(InKey, MoveTemp(Created));
		const int64 Axis = InKey.SideCells / 16;
		FineShardOwnershipAtomCapacity += Axis * Axis * Axis;
	}
	const uint64 ContentRevision = Module.GetRuntime()->GetChangeHierarchy().GetFineShardRevision(InKey);
	if (Slot->Build.NotifyContentChanged(Module.GetManifest().BaseSampleHash, Registry->Hash, Shapes->GetContentHash(),
		ContentRevision, ContentRevision))
	{
		const FVoxelTaskStamp OldStamp = Slot->SubmittedStamp;
		Scheduler.CancelMatching([OldStamp](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::BuildFineShard && Stamp == OldStamp;
		});
		Module.ReleaseGenerationFacts(Slot->Consumer);
		Slot->bSubmitted = false;
		Slot->bDependencyWake = true;
	}
	if (Slot->bSubmitted || Slot->Build.IsMeshReady() || !Slot->Build.GetFailure().IsEmpty()) return false;
	const FVoxelFineBuildAttempt Attempt = Slot->Build.BeginAttempt();
	Slot->Consumer = {EVoxelGenerationConsumerKind::FineShard, WorldEpoch, FineShardOwnerId, Attempt.Input.Incarnation,
		EVoxelGenerationSpatialKind::Bounds, InKey.Min, InKey.Bounds().Max, Attempt.Serial};
	FVoxelGenerationBounds Bounds;
	if (!CaptureBounds(InKey, Bounds)) return false;
	const auto Facts = Module.EnsureGenerationFacts(Slot->Consumer, Bounds, Demand->ServicePriority);
	if (Facts.bFailed)
	{
		Slot->Build.Complete(Attempt, false, nullptr, Facts.Error);
		Module.ReleaseGenerationFacts(Slot->Consumer);
		return false;
	}
	if (!Facts.bReady) return false;
	FVoxelFineOverlayCapture Capture;
	FString Error;
	if (!Module.CaptureFineOverlays(Bounds, Capture, Error, &Slot->Consumer)) return false;
	const uint64 CaptureBytes = VoxelBoundedAllocation::WorkerSharedBytes<FFineShardWorker>() +
		VoxelBoundedAllocation::SharedBytes<TAtomic<bool>>() +
		VoxelBoundedAllocation::SharedBytes<FVoxelFineOverlayReadResult>() +
		FMemory::QuantizeSize(sizeof(FVoxelFineShardSampler), alignof(FVoxelFineShardSampler)) +
		FMemory::QuantizeSize(sizeof(FVoxelFineShardMeshing), alignof(FVoxelFineShardMeshing));
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CaptureLease;
	if (!Scheduler.GetTerrainResources()->Acquire(Slot->Consumer, EVoxelTerrainResourcePool::Input,
		CaptureBytes, CaptureLease)) return false;
	const auto Worker = VoxelBoundedAllocation::MakeWorkerShared<FFineShardWorker>(CaptureLease, Scheduler.GetTerrainResources(),
		Generator.ToSharedRef(), Registry.ToSharedRef(), Shapes.ToSharedRef());
	Worker->CaptureLease = MoveTemp(CaptureLease);
	Worker->SamplesReady = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
	Slot->SamplesReady = Worker->SamplesReady;
	Worker->Attempt = Attempt;
	Worker->FactResources = Capture.Resources;
	Worker->RegistrationLease = Slot->RegistrationLease;
	Worker->Consumer = Slot->Consumer;
	Worker->Policy = Module.GetViewSettings().Fine;
	Worker->RecipeHash = Module.GetManifest().RecipeHash;
	Worker->WorldMinZ = Module.GetManifest().Settings.MinZ;
	Worker->WorldMaxZ = Module.GetManifest().Settings.MaxZ;
	if (!Worker->Reader.Initialize(MoveTemp(Capture), Error)) return false;
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildFineShard;
	Request.Stamp.OwnerId = FineShardOwnerId;
	Request.Stamp.OwnerIncarnation = Attempt.Input.Incarnation;
	Request.Stamp.WorldEpoch = WorldEpoch;
	Request.Stamp.Token = Attempt.Serial;
	Request.Stamp.Revision = Attempt.Input.MeshRevision;
	Request.Stamp.GeometryRevision = Attempt.Input.DataRevision;
	Request.Stamp.bHasGeometryIdentity = true;
	Request.Stamp.GeometryIdentity = {{EVoxelTerrainRepresentation::Fine, InKey.Min / InKey.SideCells,
		uint8(FMath::FloorLog2(uint32(InKey.SideCells / 16)))}, 1, Worker->WorldMinZ, Worker->WorldMaxZ,
		Attempt.Input.GeneratorIdentity, Attempt.Input.RegistryRevision};
	Request.ServiceBand = Demand->ServicePriority.Band;
	Request.SourcePriority = Demand->ServicePriority.SourcePriority;
	Request.SourceStableKey = Demand->ServicePriority.SourceStableKey;
	Request.DistanceScore = Demand->ServicePriority.DistanceCells;
	Request.ForwardScore = Demand->ServicePriority.ForwardScore;
	Request.WorkClass = Request.ServiceBand <= EVoxelTerrainServiceBand::P1 ? EVoxelWorkClass::Critical : EVoxelWorkClass::Visible;
	Request.InputBytes = CaptureBytes;
	Request.ReservedBytes = sizeof(FVoxelFineShardGeometry);
	Request.FirstRunnableSeconds = Slot->FirstQueuedSeconds;
	Request.bResumable = true;
	Request.GenerationConsumer = Worker->Consumer;
	Request.GetBlockReason = [Worker]
	{
		uint64 FactVersion = 0;
		const bool bFactsBlocked = Worker->bWaitingFacts.Load() &&
			(!Worker->FactResources->TryGetReleaseVersionExcludingOwner(Worker->Consumer, FactVersion) || FactVersion <= Worker->FactCapacityVersion.Load());
		uint64 GeometryVersion = 0;
		const bool bGeometryBlocked = Worker->bWaitingCapacity.Load() &&
			(!Worker->Resources->TryGetReleaseVersionExcludingOwner(Worker->Consumer, GeometryVersion) || GeometryVersion <= Worker->CapacityVersion.Load());
		return bGeometryBlocked || bFactsBlocked ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	};
	Request.Execute = [Worker](const TAtomic<bool>& Cancel) { return Worker->Advance(Cancel); };
	Request.Apply = [this, InKey, Attempt, Consumer = Slot->Consumer](FVoxelTaskResult&& Result)
	{
		ApplyFineShardResult(InKey, Attempt, Consumer, MoveTemp(Result));
	};
	Slot->SubmittedStamp = Request.Stamp;
	Slot->bSubmitted = Scheduler.Enqueue(MoveTemp(Request));
	return Slot->bSubmitted;
}

void FVoxelViewManager::ApplyFineShardResult(const FVoxelFineShardKey& InKey, const FVoxelFineBuildAttempt& InAttempt,
	const FVoxelGenerationConsumerKey& InConsumer, FVoxelTaskResult&& InResult)
{
	Module.ReleaseGenerationFacts(InConsumer);
	FFineShardSlot* Slot = FineShardSlots.Find(InKey);
	if (!Slot || Slot->Consumer != InConsumer || !Slot->Build.IsCurrent(InAttempt))
	{
		++TerrainWorkMetrics.Stale;
		return;
	}
	Slot->bSubmitted = false;
	const auto* Runtime = Module.GetRuntime();
	const bool bCurrent = Runtime && InResult.Stamp == Slot->SubmittedStamp && InResult.Stamp.WorldEpoch == WorldEpoch &&
		Runtime->GetChangeHierarchy().GetFineShardRevision(InKey) == InAttempt.Input.DataRevision &&
		(!InResult.bSuccess || (InResult.OverlayReadResult && Module.IsOverlayReadCurrent(*InResult.OverlayReadResult)));
	const auto Geometry = InResult.bSuccess && bCurrent ?
		StaticCastSharedPtr<const FVoxelFineShardGeometry>(InResult.CustomPayload) : nullptr;
	const auto Disposition = Slot->Build.Complete(InAttempt, InResult.bCanceled || !bCurrent, Geometry, MoveTemp(InResult.Error));
	if (Disposition == EVoxelFineAttemptDisposition::Canceled)
	{
		if (bCurrent && InResult.bCanceled) ++TerrainWorkMetrics.Canceled;
		else ++TerrainWorkMetrics.Stale;
		const auto* Exact = InKey.SideCells == 16 ? GetViewInterest().Exact.Find(InKey.Min / 16) : nullptr;
		TerrainWorkMetrics.Retried += (Exact && Exact->bFineRender) || GetViewInterest().FineShardDemands.Contains(InKey);
	}
	else if (Disposition == EVoxelFineAttemptDisposition::Stale) ++TerrainWorkMetrics.Stale;
	else if (Disposition == EVoxelFineAttemptDisposition::Failed)
	{
		++TerrainWorkMetrics.Failed;
		UE_LOG(LogTemp, Error, TEXT("Fine shard build failed: min=%s side=%d attempt=%llu error=%s"),
			*InKey.Min.ToString(), InKey.SideCells, InAttempt.Serial, *Slot->Build.GetFailure());
	}
	if (Disposition == EVoxelFineAttemptDisposition::Accepted)
	{
		OnPresentationReady(InKey);
	}
}

void FVoxelViewManager::OnContentChanged(const FIntVector& InSection)
{
	const auto Keys = Module.GetRuntime()->GetChangeHierarchy().GatherAffectedFineShards(InSection);
	for (const FVoxelFineShardKey& Key : Keys)
	{
		FFineShardSlot* Slot = FineShardSlots.Find(Key);
		if (!Slot) continue;
		const FVoxelTaskStamp OldStamp = Slot->SubmittedStamp;
		Scheduler.CancelMatching([OldStamp](EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::BuildFineShard && Stamp == OldStamp;
		});
		Module.ReleaseGenerationFacts(Slot->Consumer);
		const uint64 Revision = Module.GetRuntime()->GetChangeHierarchy().GetFineShardRevision(Key);
		Slot->Build.NotifyDataChanged(Revision, Revision);
		Slot->bSubmitted = false;
		Slot->bDependencyWake = true;
	}
}

void FVoxelViewManager::OnOwnershipChanged(const FVoxelFineShardKey& InKey)
{
	if (auto* Slot = FineShardSlots.Find(InKey))
	{
		Slot->Build.NotifyTopologyChanged();
		EnqueueFinePresentation(InKey);
	}
}

void FVoxelViewManager::OnPresentationReady(const FVoxelFineShardKey& InKey)
{
	const auto* Slot = FineShardSlots.Find(InKey);
	if (!Slot || !Slot->Build.IsMeshReady()) return;
	EnqueueFinePresentation(InKey);
	bFinePresentationDirty = true;
	const auto Bounds = InKey.Bounds();
	MarkCoverageDirty({FIntPoint(Bounds.Min.X, Bounds.Min.Y), FIntPoint(Bounds.Max.X, Bounds.Max.Y)});
}

void FVoxelViewManager::OnGenerationDependencyReady(const FVoxelGenerationConsumerKey& InConsumer)
{
	const FVoxelFineShardKey Key{InConsumer.Min, InConsumer.Max.X - InConsumer.Min.X};
	if (auto* Slot = FineShardSlots.Find(Key); Slot && Slot->Consumer == InConsumer) Slot->bDependencyWake = true;
}

void FVoxelViewManager::RetireFineShard(const FVoxelFineShardKey& InKey)
{
	FFineShardSlot* Slot = FineShardSlots.Find(InKey);
	if (!Slot) return;
	if (Slot->PresentationQueueIndex != INDEX_NONE)
	{
		FinePresentationQueues[Slot->PresentationQueueBand].RemoveAt(Slot->PresentationQueueIndex);
	}
	const FVoxelTaskStamp Stamp = Slot->SubmittedStamp;
	Scheduler.CancelMatching([Stamp](EVoxelTaskKind Kind, const FVoxelTaskStamp& Candidate)
	{
		return Kind == EVoxelTaskKind::BuildFineShard && Stamp == Candidate;
	});
	Module.ReleaseGenerationFacts(Slot->Consumer);
	FineShardSlotOrder.RemoveAt(Slot->OrderIndex);
	const int64 Axis = InKey.SideCells / 16;
	check(FineShardOwnershipAtomCapacity >= Axis * Axis * Axis);
	FineShardOwnershipAtomCapacity -= Axis * Axis * Axis;
	FineShardSlots.Remove(InKey);
}

void FVoxelViewManager::AdvanceNativeFineRetirement()
{
	const auto Snapshot = AppliedInterestSnapshot;
	if (!Snapshot || FineShardSlotOrder.IsEmpty()) return;
	const double Deadline = TerrainDeadline(0.05);
	for (int32 Count = 0; Count < FMath::Min(16, FineShardSlotOrder.GetMaxIndex()) && FPlatformTime::Seconds() < Deadline; ++Count)
	{
		NativeFineRetirementCursor %= FineShardSlotOrder.GetMaxIndex();
		const int32 Index = NativeFineRetirementCursor++;
		if (!FineShardSlotOrder.IsValidIndex(Index)) continue;
		const FVoxelFineShardKey Key = FineShardSlotOrder[Index];
		const auto* RuntimeDemand = Key.SideCells == 16 ? Snapshot->Exact.Find(Key.Min / 16) : nullptr;
		if (Key.SideCells == 16 ? RuntimeDemand && RuntimeDemand->bFineRender : Snapshot->FineShardDemands.Contains(Key)) continue;
		RetireFineShard(Key);
	}
}

bool FVoxelViewManager::RequestF0Fine(const FIntVector& InSection, const uint64 InRevision)
{
	const auto Runtime = Module.GetRuntime();
	const FVoxelSection* Section = Runtime->FindSection(InSection);
	const auto* Demand = GetViewInterest().Exact.Find(InSection);
	const auto Registry = Module.GetRegistry();
	const auto Shapes = Module.GetShapes();
	if (!Section || Section->Status != EVoxelSectionStatus::DataReady || Section->CommittedRevision != InRevision ||
		!Demand || !Demand->bFineRender || !Registry || !Shapes) return false;
	const FVoxelFineShardKey Key{InSection * 16, 16};
	FFineShardSlot* Slot = FineShardSlots.Find(Key);
	if (!Slot)
	{
		FVoxelFineMeshInput Input;
		Input.WorldEpoch = WorldEpoch;
		check(NextFineShardIncarnation < MAX_uint64);
		Input.Incarnation = ++NextFineShardIncarnation;
		Input.GeneratorIdentity = Module.GetManifest().BaseSampleHash;
		Input.RegistryRevision = Registry->Hash;
		Input.ShapeRevision = Shapes->GetContentHash();
		Input.MeshRevision = 1;
		Input.Shard = Key;
		FFineShardSlot Created;
		const auto RegistrationConsumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, FineShardOwnerId, Input.Incarnation, InSection, 1);
		Created.RegistrationLease = PinFineIndex(Module, Scheduler.GetTerrainResources(), RegistrationConsumer, Key,
			sizeof(FFineShardSlot) + 512);
		if (!Created.RegistrationLease) return false;
		Input.DataRevision = Input.OverlayRevision = Runtime->GetChangeHierarchy().GetFineShardRevision(Key);
		FString Error;
		if (!Created.Build.Initialize(Input, Error)) return false;
		Created.FirstQueuedSeconds = FPlatformTime::Seconds();
		check(NextFineShardSequence < MAX_uint64);
		Created.QueueSequence = ++NextFineShardSequence;
		Created.OrderIndex = FineShardSlotOrder.Add(Key);
		Slot = &FineShardSlots.Add(Key, MoveTemp(Created));
		++FineShardOwnershipAtomCapacity;
	}
	const uint64 ContentRevision = Runtime->GetChangeHierarchy().GetFineShardRevision(Key);
	if (Slot->Build.NotifyContentChanged(Module.GetManifest().BaseSampleHash, Registry->Hash, Shapes->GetContentHash(),
		ContentRevision, ContentRevision))
	{
		const FVoxelTaskStamp OldStamp = Slot->SubmittedStamp;
		Scheduler.CancelMatching([OldStamp](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::BuildFineShard && Stamp == OldStamp;
		});
		Module.ReleaseGenerationFacts(Slot->Consumer);
		Slot->bSubmitted = false;
		Slot->bDependencyWake = true;
	}
	if (Slot->bSubmitted || Slot->Build.IsMeshReady() || !Slot->Build.GetFailure().IsEmpty()) return false;
	const FVoxelFineBuildAttempt Attempt = Slot->Build.BeginAttempt();
	Slot->Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, FineShardOwnerId,
		Attempt.Input.Incarnation, InSection, Attempt.Serial);
	const uint64 InputBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineShardSamples>() +
		VoxelBoundedAllocation::InitialGrowBytes<uint32>(4096) + 6 * VoxelBoundedAllocation::InitialGrowBytes<uint32>(256);
	const uint64 WorkerBytes = VoxelBoundedAllocation::WorkerSharedBytes<FF0FineWorker>() +
		FMemory::QuantizeSize(sizeof(FVoxelFineShardMeshing), alignof(FVoxelFineShardMeshing));
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease, WorkerLease;
	const auto Resources = Scheduler.GetTerrainResources();
	if (!Resources->Acquire(Slot->Consumer, EVoxelTerrainResourcePool::Input, InputBytes, Lease) ||
		!Resources->Acquire(Slot->Consumer, EVoxelTerrainResourcePool::Input, WorkerBytes, WorkerLease)) return false;
	auto Samples = VoxelBoundedAllocation::MakeWorkerShared<FVoxelFineShardSamples>(Lease);
	Samples->ResourceLease = MoveTemp(Lease);
	Samples->RegistrationLease = Slot->RegistrationLease;
	Samples->Attempt = Attempt;
	if (!Module.IsAuthority())
	{
		FVoxelGenerationBounds Bounds;
		FString Error;
		const auto* Replica = Module.GetOverlayReplica();
		if (!Replica || !CaptureBounds(Key, Bounds) || !Replica->CaptureVersion(Bounds, Samples->OverlayVersion, Error)) return false;
	}
	if (!Runtime->CaptureSnapshot(InSection, Samples->Snapshot, Samples->ResourceLease)) return false;
	for (int32 Face = 0; Face < 6; ++Face)
	{
		if (Samples->Snapshot.Known[Face]) continue;
		const int32 NeighborMinZ = (InSection.Z + (Face == 4 ? 1 : Face == 5 ? -1 : 0)) * 16;
		if (NeighborMinZ >= Module.GetManifest().Settings.MaxZ || NeighborMinZ + 16 <= Module.GetManifest().Settings.MinZ)
		{
			Samples->Snapshot.Known[Face] = true;
			for (uint32& Packed : Samples->Snapshot.Halo[Face]) Packed = FVoxelBlockState().Pack();
		}
		else return false;
	}
	const FVoxelSectionStamp CapturedSectionStamp = Samples->Snapshot.Stamp;
	auto Worker = VoxelBoundedAllocation::MakeWorkerShared<FF0FineWorker>(WorkerLease, Resources, Registry.ToSharedRef(), Shapes.ToSharedRef());
	Worker->Samples = Samples;
	Worker->Consumer = Slot->Consumer;
	Worker->Policy = Module.GetViewSettings().Fine;
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildFineShard;
	Request.Stamp.OwnerId = FineShardOwnerId;
	Request.Stamp.OwnerIncarnation = Attempt.Input.Incarnation;
	Request.Stamp.WorldEpoch = WorldEpoch;
	Request.Stamp.Token = Attempt.Serial;
	Request.Stamp.Revision = Attempt.Input.MeshRevision;
	Request.Stamp.GeometryRevision = Attempt.Input.DataRevision;
	Request.Stamp.Section = InSection;
	Request.Stamp.bHasGeometryIdentity = true;
	Request.Stamp.GeometryIdentity = {{EVoxelTerrainRepresentation::Fine, InSection, 0}, 1,
		Module.GetManifest().Settings.MinZ, Module.GetManifest().Settings.MaxZ,
		Attempt.Input.GeneratorIdentity, Attempt.Input.RegistryRevision};
	Request.ServiceBand = Demand->ServicePriority.Band;
	Request.SourcePriority = Demand->ServicePriority.SourcePriority;
	Request.SourceStableKey = Demand->ServicePriority.SourceStableKey;
	Request.DistanceScore = Demand->ServicePriority.DistanceCells;
	Request.ForwardScore = Demand->ServicePriority.ForwardScore;
	Request.WorkClass = Request.ServiceBand <= EVoxelTerrainServiceBand::P1 ? EVoxelWorkClass::Critical : EVoxelWorkClass::Interactive;
	Request.FirstRunnableSeconds = Slot->FirstQueuedSeconds;
	Request.InputBytes = InputBytes + WorkerBytes;
	Request.ReservedBytes = sizeof(FVoxelFineShardGeometry);
	Request.bResumable = true;
	Request.GetBlockReason = [Worker]
	{
		uint64 Version = 0;
		return Worker->bWaitingCapacity.Load() &&
			(!Worker->Resources->TryGetReleaseVersionExcludingOwner(Worker->Consumer, Version) || Version <= Worker->CapacityVersion.Load()) ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	};
	Request.Execute = [Worker](const TAtomic<bool>& Cancel) { return Worker->Advance(Cancel); };
	Request.Apply = [this, Key, InSection, InRevision, Attempt, CapturedSectionStamp, Consumer = Slot->Consumer](FVoxelTaskResult&& Result)
	{
		FFineShardSlot* Current = FineShardSlots.Find(Key);
		const FVoxelSection* Data = Module.GetRuntime()->FindSection(InSection);
		if (!Current || Current->Consumer != Consumer || !Current->Build.IsCurrent(Attempt)) return;
		Current->bSubmitted = false;
		const bool bCurrent = Result.Stamp == Current->SubmittedStamp && Data && Data->Stamp == CapturedSectionStamp &&
			Data->CommittedRevision == InRevision && Module.GetRuntime()->GetChangeHierarchy().GetFineShardRevision(Key) == Attempt.Input.DataRevision &&
			(!Result.bSuccess || (Result.CustomPayload && Module.IsOverlayVersionCurrent(
				StaticCastSharedPtr<const FVoxelFineShardGeometry>(Result.CustomPayload)->OverlayVersion)));
		const auto Geometry = Result.bSuccess && bCurrent ? StaticCastSharedPtr<const FVoxelFineShardGeometry>(Result.CustomPayload) : nullptr;
		if (Current->Build.Complete(Attempt, Result.bCanceled || !bCurrent, Geometry, MoveTemp(Result.Error)) == EVoxelFineAttemptDisposition::Accepted)
		{
			OnPresentationReady(Key);
			const auto UpdateDerivedCoverage = [this, Key, InSection, InRevision, Input = Attempt.Input]
			{
				const auto* Slot = FineShardSlots.Find(Key);
				if (!Slot || Slot->Build.GetInput() != Input || !Slot->Build.IsMeshReady()) return;
				FineReady.Add(InSection);
				FineRevisions.Add(InSection, InRevision);
				FineLastWanted.Add(InSection, FPlatformTime::Seconds());
				IndexCoverage(FVoxelViewKey{InSection, 0});
				TrackReadyTerrainNode({InSection, 0});
				bFinePresentationDirty = bTerrainBuildStageDirty = true;
				MarkCoverageDirty(FineCoverageRect(InSection));
			};
			if (ShouldDeferCoverage(FineCoverageRect(InSection))) DeferredCoverageCallbacks.Enqueue({FineCoverageRect(InSection), UpdateDerivedCoverage});
			else UpdateDerivedCoverage();
		}
	};
	Slot->SubmittedStamp = Request.Stamp;
	Slot->bSubmitted = Scheduler.Enqueue(MoveTemp(Request));
	return Slot->bSubmitted;
}

void FVoxelViewManager::EnqueueFinePresentation(const FVoxelFineShardKey& InKey)
{
	FFineShardSlot* Slot = FineShardSlots.Find(InKey);
	if (!Slot || !Slot->Build.IsMeshReady()) return;
	const FVoxelTerrainServicePriority* Priority = nullptr;
	if (InKey.SideCells == 16)
	{
		if (const auto* Demand = GetViewInterest().Exact.Find(InKey.Min / 16); Demand && Demand->bFineRender)
			Priority = &Demand->ServicePriority;
	}
	else if (const auto* Demand = GetViewInterest().FineShardDemands.Find(InKey)) Priority = &Demand->ServicePriority;
	if (!Priority) return;
	Slot->bPresentationComplete = false;
	const uint8 Band = static_cast<uint8>(Priority->Band);
	if (Slot->PresentationQueueIndex != INDEX_NONE)
	{
		if (Band == Slot->PresentationQueueBand) return;
		FinePresentationQueues[Slot->PresentationQueueBand].RemoveAt(Slot->PresentationQueueIndex);
	}
	else Slot->PresentationQueuedAt = Slot->FirstQueuedSeconds;
	Slot->PresentationQueueBand = Band;
	Slot->PresentationQueueIndex = FinePresentationQueues[Band].Add(InKey);
}

void FVoxelViewManager::OnFineOwnershipChanged(const FIntVector& InSection)
{
	for (const int32 Side : {16, 32, 64})
	{
		FIntVector Min;
		if (!FVoxelFineSpatial::TrySnap(InSection * 16, Side, Min)) continue;
		const FVoxelFineShardKey Key{Min, Side};
		OnOwnershipChanged(Key);
	}
}

void FVoxelViewManager::PrepareFineShardPresentations()
{
	if (!AppliedInterestSnapshot || FineShardSlotOrder.IsEmpty()) return;
	FVoxelTerrainRenderer& Renderer = Publisher->GetTerrainRenderer();
	const double Deadline = TerrainDeadline(0.25);
	TArray<FVoxelTerrainServiceCandidate, TInlineAllocator<64>> Candidates;
	TArray<FVoxelFineShardKey, TInlineAllocator<64>> Keys;
	for (uint8 Band = 1; Band < static_cast<uint8>(EVoxelTerrainServiceBand::Count); ++Band)
	{
		auto& Queue = FinePresentationQueues[Band];
		for (int32 Scan = 0; Scan < FMath::Min(8, Queue.GetMaxIndex()) && FPlatformTime::Seconds() < Deadline; ++Scan)
		{
		if (Queue.IsEmpty()) break;
		FinePresentationQueueCursors[Band] %= Queue.GetMaxIndex();
		const int32 Index = FinePresentationQueueCursors[Band]++;
		if (!Queue.IsValidIndex(Index)) continue;
		const auto& Key = Queue[Index];
		FFineShardSlot* Slot = FineShardSlots.Find(Key);
		if (!Slot || !Slot->Build.IsMeshReady()) continue;
		const FVoxelTerrainServicePriority* Priority = nullptr;
		if (Key.SideCells == 16)
		{
			if (const auto* Demand = GetViewInterest().Exact.Find(Key.Min / 16); Demand && Demand->bFineRender)
				Priority = &Demand->ServicePriority;
		}
		else if (const auto* Demand = GetViewInterest().FineShardDemands.Find(Key)) Priority = &Demand->ServicePriority;
		if (!Priority) continue;
		auto& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Index = Keys.Add(Key);
		Candidate.Priority = *Priority;
		Candidate.FirstQueuedSeconds = Slot->PresentationQueuedAt;
		Candidate.Sequence = Slot->QueueSequence;
		Candidate.EstimatedMicroseconds = FineShardPresentationCostMicroseconds;
		Candidate.bRunnable = true;
		}
	}
	FVoxelTerrainServiceCapacity Capacity;
	Capacity.bAllowOverdueOpportunity = Module.GetTerrainFrameContext().IsOverdueOpportunityAvailable();
	Capacity.MaximumWaitSeconds = Module.GetViewSettings().Fine.MaximumBackgroundWaitSeconds;
	FVoxelTerrainServiceCharge Charge;
	const int32 Chosen = FineShardPresentationSelection.Select(Candidates, Capacity, FPlatformTime::Seconds(), Charge);
	if (Chosen == INDEX_NONE) return;
	if (FPlatformTime::Seconds() >= Deadline)
	{
		verify(FineShardPresentationSelection.RefundBeforeStart(Charge.Serial));
		return;
	}
	if (Charge.bOverdueOpportunity) Module.GetTerrainFrameContext().ConsumeOverdueOpportunity();
	const double Started = FPlatformTime::Seconds();
	const FVoxelFineShardKey Key = Keys[Chosen];
	FFineShardSlot& Slot = FineShardSlots.FindChecked(Key);
	const auto Geometry = Slot.Build.GetPreparedGeometry();
	const auto Resources = Scheduler.GetTerrainResources();
	const auto MakeAtom = [&](const FIntVector& Section) -> TSharedPtr<FVoxelTerrainRenderAtom, ESPMode::ThreadSafe>
	{
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainRenderAtom>() +
			VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelFineBoundarySnapshot, ESPMode::ThreadSafe>>(1) +
			128 + sizeof(TPair<FIntVector, TSharedPtr<FVoxelTerrainRenderAtom, ESPMode::ThreadSafe>>);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!Resources->Acquire(Slot.Consumer, EVoxelTerrainResourcePool::Staged, Bytes, Lease)) return nullptr;
		auto Atom = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTerrainRenderAtom>(Lease);
		Atom->Section = Section;
		Atom->FineGeometry = Geometry;
		Atom->MetadataLease = MoveTemp(Lease);
		Atom->Boundaries.Reserve(1);
		Atom->Boundaries.Add(Geometry->Boundary);
		Atom->Evidence.Owner = {Key.SideCells == 16 ? EVoxelFineOwnerKind::F0 : EVoxelFineOwnerKind::F1Plus,
			FineShardOwnerId, Geometry->Attempt.Input.Incarnation};
		const auto& Input = Geometry->Attempt.Input;
		Atom->Evidence.Content = {Input.WorldEpoch, Input.GeneratorIdentity, Module.GetManifest().RecipeHash,
			Input.RegistryRevision, Input.ShapeRevision, Input.DataRevision, Input.OverlayRevision,
			Input.MeshRevision, Geometry->GeometryIdentity, 1};
		Atom->Evidence.bGeometryReady = Atom->Evidence.bCanStage = Atom->Evidence.bKnownEmpty = true;
		Atom->Evidence.GeometryLease = Geometry;
		return Atom;
	};
	const auto Advance = [&]
	{
		if (Slot.RenderGeometry != Geometry)
		{
			// 尚未封存的旧准备包异步释放；已发行帧与 Atom 根继续保持自己的强引用。
			auto Previous = MakeShared<decltype(Slot.RenderAtoms), ESPMode::ThreadSafe>(MoveTemp(Slot.RenderAtoms));
			AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Previous] {});
			Slot.RenderGeometry = Geometry;
			Slot.RenderPieceCursor = Slot.RenderBatchCursor = Slot.RenderAtomCursor = 0;
			Slot.RenderTriangleCursor = 0;
			Slot.RenderLeaseCursor = -2;
			Slot.bRenderPrepared = false;
			Slot.bPresentationComplete = Slot.bCycleNeedsPresentation = false;
		}
		const int32 AtomSide = Key.SideCells / 16;
		const int32 AtomCount = AtomSide * AtomSide * AtomSide;
		if (Slot.RenderAtoms.Num() < AtomCount)
		{
			const int32 AtomIndex = Slot.RenderAtoms.Num();
			const FIntVector Section = Key.Min / 16 + FIntVector(AtomIndex % AtomSide,
				(AtomIndex / AtomSide) % AtomSide, AtomIndex / (AtomSide * AtomSide));
			if (auto Atom = MakeAtom(Section)) Slot.RenderAtoms.Add(Section, MoveTemp(Atom));
			return;
		}
		if (Geometry->Pieces.IsValidIndex(Slot.RenderPieceCursor))
		{
			const FVoxelFineMeshPiece& Piece = Geometry->Pieces[Slot.RenderPieceCursor];
			if (!Piece.Mesh->Batches.IsValidIndex(Slot.RenderBatchCursor))
			{
				++Slot.RenderPieceCursor;
				Slot.RenderBatchCursor = 0;
				Slot.RenderTriangleCursor = 0;
				return;
			}
			if (Piece.Mesh->Batches[Slot.RenderBatchCursor].Mesh.Triangles.IsEmpty()) { ++Slot.RenderBatchCursor; return; }
			const FIntVector Section(int32(FVoxelFineSpatial::FloorDivide(Piece.Key.Min.X, 16)),
				int32(FVoxelFineSpatial::FloorDivide(Piece.Key.Min.Y, 16)), int32(FVoxelFineSpatial::FloorDivide(Piece.Key.Min.Z, 16)));
			const auto Atom = Slot.RenderAtoms.FindRef(Section);
			if (!Atom) return;
			const uint64 RefBytes = VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe>>(Atom->Chunks.Num() + 1);
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> GrowthOverlap;
			if (Atom->Chunks.GetAllocatedSize() > 0 && !Resources->Acquire(Slot.Consumer,
				EVoxelTerrainResourcePool::Working, Atom->Chunks.GetAllocatedSize(), GrowthOverlap)) return;
			if (!Atom->MetadataLease->Resize(Atom->GetAllocatedBytes() - Atom->Chunks.GetAllocatedSize() + RefBytes + 128)) return;
			const int32 TriangleCount = Piece.Mesh->Batches[Slot.RenderBatchCursor].Mesh.Triangles.Num() / 3;
			const int32 UploadCount = FMath::Min(4096, TriangleCount - Slot.RenderTriangleCursor);
			const auto Chunk = Renderer.PrepareChunk(Slot.Consumer, Piece.Mesh, Slot.RenderBatchCursor,
				FVector(Piece.Key.Min) * Module.BlockSize(), Module.BlockSize(), Slot.RenderTriangleCursor, UploadCount);
			if (!Chunk) return;
			Atom->Chunks.Reserve(Atom->Chunks.Num() + 1);
			Atom->Chunks.Add(Chunk);
			Atom->Evidence.bKnownEmpty = false;
			Slot.RenderTriangleCursor += UploadCount;
			if (Slot.RenderTriangleCursor == TriangleCount) { ++Slot.RenderBatchCursor; Slot.RenderTriangleCursor = 0; }
			return;
		}
		Slot.bRenderPrepared = true;
		if (Slot.RenderLeaseCursor < Geometry->Pieces.Num())
		{
			const auto Lease = Slot.RenderLeaseCursor == -2 ? Geometry->MetadataLease : Slot.RenderLeaseCursor == -1 ?
				(Geometry->Boundary ? Geometry->Boundary->ResourceLease : nullptr) : Geometry->Pieces[Slot.RenderLeaseCursor].Mesh->ResourceLease;
			if (Lease && !Lease->Transfer(EVoxelTerrainResourcePool::Resident) &&
				(!Renderer.HasPresentedCoverage(FBox(FVector(Key.Bounds().Min), FVector(Key.Bounds().Max))) ||
					!Lease->Transfer(EVoxelTerrainResourcePool::ResidentOverlap))) return;
			++Slot.RenderLeaseCursor;
			return;
		}
		Slot.RenderAtomCursor %= AtomCount;
		const int32 AtomIndex = Slot.RenderAtomCursor++;
		if (AtomIndex == 0) Slot.bCycleNeedsPresentation = false;
		const FIntVector Section = Key.Min / 16 + FIntVector(AtomIndex % AtomSide,
			(AtomIndex / AtomSide) % AtomSide, AtomIndex / (AtomSide * AtomSide));
		if (Key.SideCells > 16 && GetInputRoot().FineWanted.Contains(Section) && IsFineGeometryPrepared(Section))
		{
			if (Slot.RenderAtomCursor == AtomCount && !Slot.bCycleNeedsPresentation) Slot.bPresentationComplete = true;
			return;
		}
		const auto Atom = Slot.RenderAtoms.FindRef(Section);
		if (!Atom || !Atom->IsResourceReady() || Renderer.IsLocked(Section)) { Slot.bCycleNeedsPresentation = true; return; }
		if (const auto* Existing = Renderer.FindPresentedAtom(Section); Existing &&
			Existing->Evidence.Owner == Atom->Evidence.Owner && Existing->Evidence.Content == Atom->Evidence.Content)
		{
			if (Slot.RenderAtomCursor == AtomCount && !Slot.bCycleNeedsPresentation) Slot.bPresentationComplete = true;
			return;
		}
		Slot.bCycleNeedsPresentation = true;
		const TSharedPtr<const FVoxelTerrainRenderAtom, ESPMode::ThreadSafe> ImmutableAtom = Atom;
		FGuid Transaction;
		const auto* RuntimeDemand = GetViewInterest().Exact.Find(Section);
		const bool bRequiresCollision = RuntimeDemand && (RuntimeDemand->bCollision || RuntimeDemand->bWarmupCollision);
		Renderer.PrepareAtomTransaction(MakeArrayView(&ImmutableAtom, 1), AppliedInterestRevision, bRequiresCollision,
			[this, Key, Input = Geometry->Attempt.Input](const FIntVector& CheckSection, const FVoxelFineOwnerEvidence& Evidence)
			{
				const auto* Current = FineShardSlots.Find(Key);
				const bool bDemanded = Key.SideCells == 16 ? GetInputRoot().FineWanted.Contains(CheckSection) :
					GetViewInterest().FineShardDemands.Contains(Key) && (!GetInputRoot().FineWanted.Contains(CheckSection) || !IsFineGeometryPrepared(CheckSection));
				return bDemanded && Current && Current->Build.GetInput() == Input && Current->Build.IsMeshReady() &&
					Module.IsOverlayVersionCurrent(Current->Build.GetPreparedGeometry()->OverlayVersion) &&
					Evidence.Content.WorldEpoch == WorldEpoch && Evidence.Content.SampleStepCells == 1 &&
					Module.GetRuntime()->GetChangeHierarchy().GetFineShardRevision(Key) == Input.DataRevision;
			},
			[this](const FIntVector& CheckSection, const FVoxelFineOwnerEvidence&) { return Module.IsCollisionReady(CheckSection); },
			[this, Section] { OnPresentationReady(Section); }, Transaction, Candidates[Chosen].Priority, Slot.FirstQueuedSeconds);
	};
	Advance();
	if (Slot.bPresentationComplete && Slot.PresentationQueueIndex != INDEX_NONE)
	{
		FinePresentationQueues[Slot.PresentationQueueBand].RemoveAt(Slot.PresentationQueueIndex);
		Slot.PresentationQueueIndex = INDEX_NONE;
	}
	const double Actual = FMath::Max(1.0, (FPlatformTime::Seconds() - Started) * 1000000.0);
	verify(FineShardPresentationSelection.Settle(Charge.Serial, Actual));
	FineShardPresentationCostMicroseconds = FMath::Lerp(FineShardPresentationCostMicroseconds, Actual, 0.15);
}

void FVoxelViewManager::OnPresentationReady(const FIntVector& InSection)
{
	if (ShouldDeferCoverage(FineCoverageRect(InSection)))
	{
		DeferredCoverageCallbacks.Enqueue({FineCoverageRect(InSection), [this, InSection] { OnPresentationReady(InSection); }});
		return;
	}
	const auto* Actual = Publisher->GetTerrainRenderer().FindPresentedAtom(InSection);
	if (!Actual)
	{
		PresentedFineSectionOwners.Remove(InSection);
		VisibleTerrainNodes.Remove({InSection, 0});
		IndexCoverage(FVoxelViewKey{InSection, 0});
		MarkCoverageDirty(FineCoverageRect(InSection));
		bTerrainBuildStageDirty = true;
		return;
	}
	if (Actual->Stamp.WorldEpoch != WorldEpoch) return;
	const auto Geometry = StaticCastSharedPtr<const FVoxelFineShardGeometry>(Actual->Evidence.GeometryLease);
	if (!Geometry) return;
	const FVoxelFinePageKey* Page = Geometry->Attempt.Input.Shard.SideCells == 16 ? GetInputRoot().FinePagePlan.SectionOwners.Find(InSection) :
		GetViewInterest().FineShardOwners.Find(Geometry->Attempt.Input.Shard);
	if (Page) PresentedFineSectionOwners.Add(InSection, *Page);
	const FVoxelViewKey Node{InSection, 0};
	CommitTerrainOwnership({0, InSection, 0}, MakeArrayView(&Node, 1));
	IndexCoverage(Node);
	MarkCoverageDirty(FineCoverageRect(InSection));
	bTerrainBuildStageDirty = true;
}

bool FVoxelViewManager::IsFineGeometryPrepared(const FIntVector& InSection) const
{
	for (const int32 Side : {16, 32, 64})
	{
		if ((Side == 16) != GetInputRoot().FineWanted.Contains(InSection)) continue;
		FIntVector Min;
		if (!FVoxelFineSpatial::TrySnap(InSection * 16, Side, Min)) continue;
		const FVoxelFineShardKey Key{Min, Side};
		const auto* Slot = FineShardSlots.Find(Key);
		if (Slot && Slot->Build.IsMeshReady() && Module.IsOverlayVersionCurrent(Slot->Build.GetPreparedGeometry()->OverlayVersion) &&
			Slot->Build.GetInput().DataRevision ==
			Module.GetRuntime()->GetChangeHierarchy().GetFineShardRevision(Key)) return true;
	}
	return false;
}

bool FVoxelViewManager::IsLatestFinePresented(const FIntVector& InSection) const
{
	const auto* Actual = Publisher->GetTerrainRenderer().FindPresentedAtom(InSection);
	if (!Actual || Actual->Stamp.WorldEpoch != WorldEpoch || Actual->Evidence.Content.SampleStepCells != 1 ||
		(Actual->Evidence.Owner.Kind != EVoxelFineOwnerKind::F0 && Actual->Evidence.Owner.Kind != EVoxelFineOwnerKind::F1Plus)) return false;
	const auto Geometry = StaticCastSharedPtr<const FVoxelFineShardGeometry>(Actual->Evidence.GeometryLease);
	return Geometry && Module.IsOverlayVersionCurrent(Geometry->OverlayVersion) &&
		Actual->Evidence.Content.GeneratorIdentity == Module.GetManifest().BaseSampleHash &&
		Actual->Evidence.Content.RecipeRevision == Module.GetManifest().RecipeHash &&
		Actual->Evidence.Content.RegistryRevision == Module.GetRegistry()->Hash &&
		Actual->Evidence.Content.ShapeRevision == Module.GetShapes()->GetContentHash() &&
		Geometry->Attempt.Input.DataRevision == Module.GetRuntime()->GetChangeHierarchy().GetFineShardRevision(Geometry->Attempt.Input.Shard);
}

TSharedPtr<const IVoxelTerrainPresentedSnapshot, ESPMode::ThreadSafe> FVoxelViewManager::CapturePresentedSnapshot() const
{
	return Publisher->GetTerrainRenderer().CapturePresentedSnapshot();
}
