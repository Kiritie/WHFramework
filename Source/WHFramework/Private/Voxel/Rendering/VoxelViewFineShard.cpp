#include "Voxel/Rendering/VoxelViewManager.h"

#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Rendering/VoxelTerrainRenderer.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Network/VoxelOverlayReplica.h"
#include "Voxel/Runtime/VoxelFineOverlayRead.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/VoxelModule.h"

struct FVoxelViewManager::FF0FineWorker
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkerLease;
	TSharedPtr<const FVoxelFineShardSamples, ESPMode::ThreadSafe> Samples;
	TUniquePtr<FVoxelFineShardMeshing> Mesher;
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
	TSharedRef<const FVoxelShapeRegistry, ESPMode::ThreadSafe> Shapes;
	FVoxelGenerationConsumerKey Consumer;
	FVoxelFineStreamingPolicy Policy;
	FVoxelSectionStamp SectionStamp;
	uint64 Revision = 0;
	bool bSnapshotCaptured = false;
	uint8 NextHalo = 0;
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
		const FVoxelGenerationRequiredSourceScope WorkerSource(&Resources.Get(), WorkerLease.Get());
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
			if (!Mesher->ResumeCapacity(BeforeCapacityBuild, Result.Error))
			{
				if (!Result.Error.StartsWith(TEXT("CapacityBlocked"))) return Result;
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

struct FVoxelViewManager::FFineShardWorker
{
	TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> SamplesReady;
	TSharedPtr<const void, ESPMode::ThreadSafe> RegistrationLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CaptureLease;
	TOptional<FVoxelFineOverlayCapture> PendingCapture;
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
		const FVoxelGenerationRequiredSourceScope WorkerSource(&Resources.Get(), CaptureLease.Get());
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
			if (Mesher && !Mesher->ResumeCapacity(BeforeCapacityBuild, Result.Error))
			{
				if (Result.Error.StartsWith(TEXT("CapacityBlocked"))) return WaitCapacity();
				return Result;
			}
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

namespace
{
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


bool FVoxelViewManager::ReserveFineShardSlots(const int32 InCount, FString& OutError)
{
	if (InCount < 0)
	{
		OutError = TEXT("InvalidRequest: Fine slot count");
		return false;
	}
	if (InCount <= FineShardBackingCapacity) return true;
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Owner = FVoxelGenerationConsumerKey::Section(WorldEpoch, FineShardOwnerId, 1, {}, 1);
	const uint64 Before = Resources->ReleaseVersionExcludingOwner(Owner);
	if (InCount >= FineShardBackingRejectedCount &&
		(FineShardBackingAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ||
			(FineShardBackingAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked && Before <= FineShardBackingReleaseVersion)))
	{
		OutError = FineShardBackingAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked ?
			TEXT("CapacityBlocked: Fine slot backing waits for an external release") : TEXT("Oversized: Fine slot backing is unreachable");
		return false;
	}
	const int32 Capacity = InCount;
	uint64 FinalBytes = FineShardSlots.GetAllocatedSize() + FineShardSlotOrder.GetAllocatedSize();
	for (const auto& Queue : FinePresentationQueues) FinalBytes += Queue.GetAllocatedSize();
	uint64 PeakBytes = FinalBytes;
	const auto CountGrowth = [&FinalBytes, &PeakBytes](const uint64 OldBytes, const uint64 NewBytes)
	{
		if (NewBytes <= OldBytes) return;
		PeakBytes = FMath::Max(PeakBytes, FinalBytes + NewBytes);
		FinalBytes = FinalBytes - OldBytes + NewBytes;
	};
	CountGrowth(FineShardSlots.GetAllocatedSize(), VoxelBoundedAllocation::MapBytes<FVoxelFineShardKey, FFineShardSlot>(Capacity));
	const uint64 SparseBytes = VoxelBoundedAllocation::SparseArrayBytes<FVoxelFineShardKey>(Capacity);
	CountGrowth(FineShardSlotOrder.GetAllocatedSize(), SparseBytes);
	for (const auto& Queue : FinePresentationQueues) CountGrowth(Queue.GetAllocatedSize(), SparseBytes);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Candidate;
	FineShardBackingAdmission = FineShardBackingLease ? FineShardBackingLease->TryResize(PeakBytes) :
		Resources->TryAcquire(Owner, EVoxelTerrainResourcePool::Input, PeakBytes, Candidate);
	if (FineShardBackingAdmission != EVoxelTerrainResourceAdmission::Granted)
	{
		FineShardBackingRejectedCount = InCount;
		FineShardBackingReleaseVersion = Before;
		OutError = FineShardBackingAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked ?
			TEXT("CapacityBlocked: Fine slot backing") : FineShardBackingAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
			TEXT("Oversized: Fine slot backing is unreachable") : TEXT("InvalidRequest: Fine slot backing resource domain");
		return false;
	}
	FineShardSlots.Reserve(Capacity);
	FineShardSlotOrder.Reserve(Capacity);
	uint64 ActualBytes = FineShardSlots.GetAllocatedSize() + FineShardSlotOrder.GetAllocatedSize();
	for (auto& Queue : FinePresentationQueues)
	{
		Queue.Reserve(Capacity);
		ActualBytes += Queue.GetAllocatedSize();
	}
	check(ActualBytes <= FinalBytes);
	if (Candidate) FineShardBackingLease = MoveTemp(Candidate);
	verify(FineShardBackingLease->Resize(ActualBytes));
	FineShardBackingCapacity = Capacity;
	FineShardBackingRejectedCount = 0;
	OutError.Reset();
	return true;
}

bool FVoxelViewManager::EnsureFineShardRegistration(const FVoxelFineShardKey& InKey)
{
	auto* Slot = FineShardSlots.Find(InKey);
	if (!Slot || !Slot->Build.GetFailure().IsEmpty() || IsFineShardAdmissionWaiting(*Slot)) return false;
	if (Slot->RegistrationLease) return true;
	const auto Resources = Scheduler.GetRuntimeResources();
	const auto Control = Module.GetRuntime()->GetChangeHierarchy().GetControlConsumer();
	const uint64 Before = Resources->ReleaseVersionExcludingOwner(Control);
	FString Error;
	if (Module.GetRuntime()->GetChangeHierarchy().PinFineShard(InKey, Slot->RegistrationLease, Error)) return true;
	const auto Attempt = Slot->Build.BeginAttempt();
	Slot->Consumer = {EVoxelGenerationConsumerKind::FineShard, WorldEpoch, FineShardOwnerId, Attempt.Input.Incarnation,
		EVoxelGenerationSpatialKind::Bounds, InKey.Min, InKey.Bounds().Max, Attempt.Serial};
	Slot->SubmittedStamp.WorldEpoch = WorldEpoch;
	Slot->SubmittedStamp.OwnerId = FineShardOwnerId;
	Slot->SubmittedStamp.OwnerIncarnation = Attempt.Input.Incarnation;
	Slot->SubmittedStamp.Token = Attempt.Serial;
	Slot->SubmittedStamp.Revision = Attempt.Input.MeshRevision;
	if (Error.StartsWith(TEXT("CapacityBlocked")))
	{
		Scheduler.ObserveDemand(EVoxelTaskKind::BuildFineShard, Slot->SubmittedStamp, EVoxelTaskBlockReason::InputMemory);
		WaitFineShardAdmission(*Slot, Resources, Before, &Control);
		return false;
	}
	return RejectFineShardAdmission(InKey, Attempt, Error, Before);
}

void FVoxelViewManager::ClearFineShardAdmission(FFineShardSlot& InSlot)
{
	InSlot.PendingRequest.Reset();
	InSlot.PendingWorker.Reset();
	InSlot.PendingF0Worker.Reset();
	InSlot.PendingCapture.Reset();
	InSlot.SamplesReady.Reset();
	InSlot.SamplesReadyLease.Reset();
	InSlot.AdmissionResources.Reset();
	InSlot.AdmissionConsumer = {};
	InSlot.bWaitingAdmissionCapacity = false;
	InSlot.AdmissionCapacityVersion = 0;
}

bool FVoxelViewManager::IsFineShardAdmissionWaiting(FFineShardSlot& InSlot)
{
	if (!InSlot.bWaitingAdmissionCapacity) return false;
	uint64 Version = 0;
	if (!InSlot.AdmissionResources ||
		!InSlot.AdmissionResources->TryGetReleaseVersionExcludingOwner(InSlot.AdmissionConsumer, Version) ||
		Version <= InSlot.AdmissionCapacityVersion) return true;
	InSlot.bWaitingAdmissionCapacity = false;
	InSlot.AdmissionResources.Reset();
	return false;
}

void FVoxelViewManager::WaitFineShardAdmission(FFineShardSlot& InSlot,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources, const uint64 InVersion,
	const FVoxelGenerationConsumerKey* InCapacityOwner)
{
	InSlot.AdmissionResources = InResources;
	InSlot.AdmissionConsumer = InCapacityOwner ? *InCapacityOwner : InSlot.Consumer;
	InSlot.AdmissionCapacityVersion = InVersion;
	InSlot.bWaitingAdmissionCapacity = true;
}

bool FVoxelViewManager::RejectFineShardAdmission(const FVoxelFineShardKey& InKey,
	const FVoxelFineBuildAttempt& InAttempt, const FString& InError, const uint64 InCapacityVersion)
{
	auto* Slot = FineShardSlots.Find(InKey);
	if (!Slot || !Slot->Build.IsCurrent(InAttempt)) return false;
	const FVoxelTaskStamp Stamp = Slot->PendingRequest.IsSet() ? Slot->PendingRequest->Stamp : Slot->SubmittedStamp;
	const bool bTemporary = InError.StartsWith(TEXT("CapacityBlocked"));
	const bool bOversized = InError.StartsWith(TEXT("Oversized")) || InError.StartsWith(TEXT("CapacityUnachievable"));
	Scheduler.ObserveDemand(EVoxelTaskKind::BuildFineShard, Stamp,
		bTemporary ? EVoxelTaskBlockReason::InputMemory : bOversized ? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::TerminalFailure);
	if (bTemporary)
	{
		WaitFineShardAdmission(*Slot, Scheduler.GetTerrainResources(), InCapacityVersion);
		return false;
	}
	const auto Consumer = Slot->Consumer;
	ClearFineShardAdmission(*Slot);
	Slot->bSubmitted = false;
	Slot->Build.Complete(InAttempt, false, nullptr, InError);
	Module.ReleaseGenerationFacts(Consumer);
	return false;
}

void FVoxelViewManager::CancelFineShardAdmission(const FVoxelFineShardKey& InKey)
{
	auto* Slot = FineShardSlots.Find(InKey);
	if (!Slot) return;
	const auto Stamp = Slot->SubmittedStamp;
	const auto Consumer = Slot->Consumer;
	ClearFineShardAdmission(*Slot);
	Slot->Consumer = {};
	Slot->bSubmitted = false;
	Slot->bDependencyWake = true;
	Scheduler.CancelMatching([Stamp](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Candidate)
	{
		return Kind == EVoxelTaskKind::BuildFineShard && Candidate == Stamp;
	});
	Module.ReleaseGenerationFacts(Consumer);
}

bool FVoxelViewManager::SubmitFineShardAdmission(const FVoxelFineShardKey& InKey,
	const FVoxelFineBuildAttempt& InAttempt)
{
	auto* Slot = FineShardSlots.Find(InKey);
	if (!Slot || !Slot->PendingRequest.IsSet() || !Slot->Build.IsCurrent(InAttempt) ||
		IsFineShardAdmissionWaiting(*Slot)) return false;
	const auto Worker = Slot->PendingWorker;
	const auto F0Worker = Slot->PendingF0Worker;
	check(bool(Worker) != bool(F0Worker));
	auto& Request = Slot->PendingRequest.GetValue();
	const auto Consumer = Slot->Consumer;
	const auto Resources = Scheduler.GetTerrainResources();
	const uint64 BeforeBinding = Resources->ReleaseVersionExcludingOwner(Consumer);
	FString Error;
	if (Worker && Worker->PendingCapture.IsSet())
	{
		const auto FactResources = Worker->FactResources.ToSharedRef();
		const uint64 BeforeRead = FactResources->ReleaseVersionExcludingOwner(Consumer);
		if (!Worker->Reader.Initialize(MoveTemp(Worker->PendingCapture.GetValue()), Error))
		{
			if (!Error.StartsWith(TEXT("CapacityBlocked"))) return RejectFineShardAdmission(InKey, InAttempt, Error, BeforeRead);
			Scheduler.ObserveDemand(Request.Kind, Request.Stamp, EVoxelTaskBlockReason::Memory);
			WaitFineShardAdmission(*Slot, FactResources, BeforeRead);
			return false;
		}
		Worker->PendingCapture.Reset();
	}
	const FVoxelTerrainServicePriority* Priority = nullptr;
	if (InKey.SideCells == 16)
	{
		if (const auto* Demand = GetViewInterest().Exact.Find(InKey.Min / 16); Demand && Demand->bFineRender)
			Priority = &Demand->ServicePriority;
	}
	else if (const auto* Demand = GetViewInterest().FineShardDemands.Find(InKey)) Priority = &Demand->ServicePriority;
	if (!Priority) return false;
	Request.ServiceBand = Priority->Band;
	Request.SourcePriority = Priority->SourcePriority;
	Request.SourceStableKey = Priority->SourceStableKey;
	Request.DistanceScore = Priority->DistanceCells;
	Request.ForwardScore = Priority->ForwardScore;
	Request.WorkClass = Request.ServiceBand <= EVoxelTerrainServiceBand::P1 ? EVoxelWorkClass::Critical :
		InKey.SideCells == 16 ? EVoxelWorkClass::Interactive : EVoxelWorkClass::Visible;
	if (!Request.bBlockBound && !Request.BindGetBlockReason(Resources, Consumer, [Worker, F0Worker]
		{
			uint64 GeometryVersion = 0;
			if (F0Worker)
			{
				return F0Worker->bWaitingCapacity.Load() &&
					(!F0Worker->Resources->TryGetReleaseVersionExcludingOwner(F0Worker->Consumer, GeometryVersion) ||
					GeometryVersion <= F0Worker->CapacityVersion.Load()) ? EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
			}
			uint64 FactVersion = 0;
			const bool bFactsBlocked = Worker->bWaitingFacts.Load() &&
				(!Worker->FactResources->TryGetReleaseVersionExcludingOwner(Worker->Consumer, FactVersion) ||
				FactVersion <= Worker->FactCapacityVersion.Load());
			const bool bGeometryBlocked = Worker->bWaitingCapacity.Load() &&
				(!Worker->Resources->TryGetReleaseVersionExcludingOwner(Worker->Consumer, GeometryVersion) ||
				GeometryVersion <= Worker->CapacityVersion.Load());
			return bGeometryBlocked || bFactsBlocked ? EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
		}, Error)) return RejectFineShardAdmission(InKey, InAttempt, Error, BeforeBinding);
	if (!Request.bExecuteBound && !Request.BindExecute(Resources, Consumer,
		[Worker, F0Worker](const TAtomic<bool>& Cancel)
		{
			return F0Worker ? F0Worker->Advance(Cancel) : Worker->Advance(Cancel);
		}, Error)) return RejectFineShardAdmission(InKey, InAttempt, Error, BeforeBinding);
	const auto SectionStamp = F0Worker ? F0Worker->SectionStamp : FVoxelSectionStamp{};
	const uint64 Revision = F0Worker ? F0Worker->Revision : 0;
	if (!Request.bApplyBound && !Request.BindApply(Resources, Consumer,
		[this, InKey, InAttempt, Consumer, SectionStamp, Revision](FVoxelTaskResult&& Result)
		{
			if (InKey.SideCells == 16)
				ApplyF0FineShardResult(InKey, Revision, SectionStamp, InAttempt, Consumer, MoveTemp(Result));
			else ApplyFineShardResult(InKey, InAttempt, Consumer, MoveTemp(Result));
		}, Error)) return RejectFineShardAdmission(InKey, InAttempt, Error, BeforeBinding);
	FVoxelTaskRequest Submission = MoveTemp(Request);
	Slot->PendingRequest.Reset();
	Slot->PendingWorker.Reset();
	Slot->PendingF0Worker.Reset();
	Slot->SubmittedStamp = Submission.Stamp;
	Slot->bSubmitted = true;
	if (Scheduler.Enqueue(MoveTemp(Submission))) return true;
	Slot = FineShardSlots.Find(InKey);
	if (!Slot || Slot->Consumer != Consumer || !Slot->Build.IsCurrent(InAttempt) || !Slot->bSubmitted) return false;
	Slot->bSubmitted = false;
	Slot->PendingWorker = Worker;
	Slot->PendingF0Worker = F0Worker;
	Slot->PendingRequest.Emplace(MoveTemp(Submission));
	const auto Reason = Slot->PendingRequest->Blocked;
	if (Reason == EVoxelTaskBlockReason::Oversized || Reason == EVoxelTaskBlockReason::TerminalFailure)
		return RejectFineShardAdmission(InKey, InAttempt, Reason == EVoxelTaskBlockReason::Oversized ?
			TEXT("Oversized: Fine native task admission is unreachable") : TEXT("InvalidRequest: Fine native task admission rejected"), BeforeBinding);
	if (Reason == EVoxelTaskBlockReason::Dependency && Scheduler.Has(Slot->SubmittedStamp, EVoxelTaskKind::BuildFineShard))
	{
		Slot->PendingRequest.Reset();
		Slot->PendingWorker.Reset();
		Slot->PendingF0Worker.Reset();
		Slot->bSubmitted = true;
		return true;
	}
	WaitFineShardAdmission(*Slot, Resources, BeforeBinding);
	return false;
}

void FVoxelViewManager::ApplyF0FineShardResult(const FVoxelFineShardKey& InKey, const uint64 InRevision,
	const FVoxelSectionStamp& InSectionStamp, const FVoxelFineBuildAttempt& InAttempt,
	const FVoxelGenerationConsumerKey& InConsumer, FVoxelTaskResult&& InResult)
{
	const FIntVector Section = InKey.Min / 16;
	auto* Current = FineShardSlots.Find(InKey);
	const auto* Runtime = Module.GetRuntime();
	const FVoxelSection* Data = Runtime ? Runtime->FindSection(Section) : nullptr;
	if (!Current || Current->Consumer != InConsumer || !Current->Build.IsCurrent(InAttempt)) return;
	Current->bSubmitted = false;
	const bool bCurrent = InResult.Stamp == Current->SubmittedStamp && Data && Data->Stamp == InSectionStamp &&
		Data->CommittedRevision == InRevision && Runtime->GetChangeHierarchy().GetFineShardRevision(InKey) == InAttempt.Input.DataRevision &&
		(!InResult.bSuccess || (InResult.CustomPayload && Module.IsOverlayVersionCurrent(
			StaticCastSharedPtr<const FVoxelFineShardGeometry>(InResult.CustomPayload)->OverlayVersion)));
	const auto Geometry = InResult.bSuccess && bCurrent ? StaticCastSharedPtr<const FVoxelFineShardGeometry>(InResult.CustomPayload) : nullptr;
	if (Current->Build.Complete(InAttempt, InResult.bCanceled || !bCurrent, Geometry, MoveTemp(InResult.Error)) !=
		EVoxelFineAttemptDisposition::Accepted) return;
	OnPresentationReady(InKey);
	const auto UpdateDerivedCoverage = [this, InKey, Section, InRevision, Input = InAttempt.Input]
	{
		const auto* Slot = FineShardSlots.Find(InKey);
		if (!Slot || Slot->Build.GetInput() != Input || !Slot->Build.IsMeshReady()) return;
		FineReady.Add(Section);
		FineRevisions.Add(Section, InRevision);
		FineLastWanted.Add(Section, FPlatformTime::Seconds());
		IndexCoverage(FVoxelViewKey{Section, 0});
		TrackReadyTerrainNode({Section, 0});
		bFinePresentationDirty = bTerrainBuildStageDirty = true;
		MarkCoverageDirty(FineCoverageRect(Section));
	};
	if (ShouldDeferCoverage(FineCoverageRect(Section)))
		DeferredCoverageCallbacks.Enqueue({FineCoverageRect(Section), UpdateDerivedCoverage});
	else UpdateDerivedCoverage();
}

void FVoxelViewManager::ProcessFineShardAdmissions()
{
	const auto& Interest = GetViewInterest();
	if (!Interest.PublicationInputs || Interest.PublicationInputs->FineShards.IsEmpty()) return;
	const auto& Keys = Interest.PublicationInputs->FineShards;
	const double Deadline = TerrainDeadline(0.5);
	TArray<FVoxelTerrainServiceCandidate, TInlineAllocator<64>> Candidates;
	TArray<FVoxelFineShardKey, TInlineAllocator<64>> Captured;
	for (int32 Scans = 0; Scans < 64 && FPlatformTime::Seconds() < Deadline; ++Scans)
	{
		if (FineShardAdmissionCursor >= Keys.Num()) FineShardAdmissionCursor = 0;
		const FVoxelFineShardKey Key = Keys[FineShardAdmissionCursor++];
		const auto* Demand = Interest.FineShardDemands.Find(Key);
		if (!Demand) continue;
		FFineShardSlot* Slot = FineShardSlots.Find(Key);
		if (Slot && (Slot->bSubmitted || Slot->Build.IsMeshReady() || !Slot->Build.GetFailure().IsEmpty() ||
			IsFineShardAdmissionWaiting(*Slot) || (!Slot->PendingRequest.IsSet() && !Slot->bDependencyWake))) continue;
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
	const auto Priority = Demand->ServicePriority;
	FFineShardSlot* Slot = FineShardSlots.Find(InKey);
	if (!Slot)
	{
		FString Error;
		if (FineShardSlots.Num() == MAX_int32 || !ReserveFineShardSlots(FineShardSlots.Num() + 1, Error)) return false;
		FVoxelFineMeshInput Input;
		Input.WorldEpoch = WorldEpoch;
		check(NextFineShardIncarnation < MAX_uint64);
		Input.Incarnation = ++NextFineShardIncarnation;
		Input.GeneratorIdentity = Module.GetManifest().BaseSampleHash;
		Input.RegistryRevision = Registry->Hash;
		Input.ShapeRevision = Shapes->GetContentHash();
		Input.Shard = InKey;
		Input.MeshRevision = 1;
		Input.DataRevision = Input.OverlayRevision = Module.GetRuntime()->GetChangeHierarchy().GetFineShardRevision(InKey);
		FFineShardSlot Created;
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
		const auto Input = Slot->Build.GetInput();
		CancelFineShardAdmission(InKey);
		Slot = FineShardSlots.Find(InKey);
		if (!Slot || Slot->Build.GetInput() != Input) return false;
	}
	if (!EnsureFineShardRegistration(InKey)) return false;
	if (Slot->bSubmitted || Slot->Build.IsMeshReady() || !Slot->Build.GetFailure().IsEmpty() ||
		IsFineShardAdmissionWaiting(*Slot)) return false;
	const auto Attempt = Slot->Build.BeginAttempt();
	Slot->Consumer = {EVoxelGenerationConsumerKind::FineShard, WorldEpoch, FineShardOwnerId, Attempt.Input.Incarnation,
		EVoxelGenerationSpatialKind::Bounds, InKey.Min, InKey.Bounds().Max, Attempt.Serial};
	Slot->SubmittedStamp.WorldEpoch = WorldEpoch;
	Slot->SubmittedStamp.OwnerId = FineShardOwnerId;
	Slot->SubmittedStamp.OwnerIncarnation = Attempt.Input.Incarnation;
	Slot->SubmittedStamp.Token = Attempt.Serial;
	Slot->SubmittedStamp.Revision = Attempt.Input.MeshRevision;
	if (Slot->PendingRequest.IsSet()) return SubmitFineShardAdmission(InKey, Attempt);
	if (!Slot->bDependencyWake) return false;
	FVoxelGenerationBounds Bounds;
	if (!CaptureBounds(InKey, Bounds))
		return RejectFineShardAdmission(InKey, Attempt, TEXT("InvalidRequest: Fine capture bounds overflow"), 0);
	const auto Consumer = Slot->Consumer;
	const auto FactResources = Scheduler.GetRuntimeResources();
	const uint64 BeforeFacts = FactResources->ReleaseVersionExcludingOwner(Consumer);
	const auto Facts = Module.EnsureGenerationFacts(Consumer, Bounds, Priority);
	Slot = FineShardSlots.Find(InKey);
	if (!Slot || Slot->Consumer != Consumer || !Slot->Build.IsCurrent(Attempt)) return false;
	if (Facts.bFailed) return RejectFineShardAdmission(InKey, Attempt, Facts.Error, 0);
	if (!Facts.bReady)
	{
		if (Facts.Error.StartsWith(TEXT("CapacityBlocked"))) WaitFineShardAdmission(*Slot, FactResources, BeforeFacts);
		else Slot->bDependencyWake = false;
		return false;
	}
	FString Error;
	if (!Slot->PendingCapture.IsSet())
	{
		FVoxelFineOverlayCapture Capture;
		if (!Module.CaptureFineOverlays(Bounds, Capture, Error, &Consumer))
		{
			if (Error.StartsWith(TEXT("CapacityBlocked"))) WaitFineShardAdmission(*Slot, FactResources, BeforeFacts);
			else if (!Error.StartsWith(TEXT("WaitingOverlay"))) return RejectFineShardAdmission(InKey, Attempt, Error, 0);
			return false;
		}
		Slot->PendingCapture.Emplace(MoveTemp(Capture));
	}
	const uint64 CaptureBytes = VoxelBoundedAllocation::WorkerSharedBytes<FFineShardWorker>() +
		VoxelBoundedAllocation::SharedBytes<TAtomic<bool>>() +
		VoxelBoundedAllocation::SharedBytes<FVoxelFineOverlayReadResult>() +
		FMemory::QuantizeSize(sizeof(FVoxelFineShardSampler), alignof(FVoxelFineShardSampler)) +
		FMemory::QuantizeSize(sizeof(FVoxelFineShardMeshing), alignof(FVoxelFineShardMeshing));
	const auto Resources = Scheduler.GetTerrainResources();
	const uint64 BeforeCapture = Resources->ReleaseVersionExcludingOwner(Consumer);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CaptureLease;
	const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, CaptureBytes, CaptureLease, 0, 0, 1);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked) WaitFineShardAdmission(*Slot, Resources, BeforeCapture);
		else return RejectFineShardAdmission(InKey, Attempt, Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
			TEXT("Oversized: Fine worker input is unreachable") : TEXT("InvalidRequest: Fine worker input domain"), BeforeCapture);
		return false;
	}
	const auto Worker = VoxelBoundedAllocation::MakeWorkerShared<FFineShardWorker>(CaptureLease, Resources,
		Generator.ToSharedRef(), Registry.ToSharedRef(), Shapes.ToSharedRef());
	Worker->CaptureLease = MoveTemp(CaptureLease);
	Worker->SamplesReady = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
	Slot->SamplesReadyLease = Worker->CaptureLease;
	Slot->SamplesReady = Worker->SamplesReady;
	Worker->Attempt = Attempt;
	Worker->FactResources = Slot->PendingCapture->Resources;
	Worker->RegistrationLease = Slot->RegistrationLease;
	Worker->Consumer = Consumer;
	Worker->Policy = Module.GetViewSettings().Fine;
	Worker->RecipeHash = Module.GetManifest().RecipeHash;
	Worker->WorldMinZ = Module.GetManifest().Settings.MinZ;
	Worker->WorldMaxZ = Module.GetManifest().Settings.MaxZ;
	Worker->PendingCapture.Emplace(MoveTemp(Slot->PendingCapture.GetValue()));
	Slot->PendingCapture.Reset();
	Slot->PendingWorker = Worker;
	Slot->PendingRequest.Emplace();
	auto& Request = Slot->PendingRequest.GetValue();
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
	Request.ServiceBand = Priority.Band;
	Request.SourcePriority = Priority.SourcePriority;
	Request.SourceStableKey = Priority.SourceStableKey;
	Request.DistanceScore = Priority.DistanceCells;
	Request.ForwardScore = Priority.ForwardScore;
	Request.WorkClass = Request.ServiceBand <= EVoxelTerrainServiceBand::P1 ? EVoxelWorkClass::Critical : EVoxelWorkClass::Visible;
	Request.InputBytes = CaptureBytes;
	Request.ReservedBytes = sizeof(FVoxelFineShardGeometry);
	Request.FirstRunnableSeconds = Slot->FirstQueuedSeconds;
	Request.bResumable = true;
	Request.GenerationConsumer = Worker->Consumer;
	Request.SetRequiredBindingInput(Worker->CaptureLease->Bytes(), 2);
	return SubmitFineShardAdmission(InKey, Attempt);
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
	Module.GetRuntime()->GetChangeHierarchy().VisitAffectedFineShards(InSection, [this](const FVoxelFineShardKey& Key)
	{
		if (auto* Slot = FineShardSlots.Find(Key))
		{
			const uint64 Revision = Module.GetRuntime()->GetChangeHierarchy().GetFineShardRevision(Key);
			Slot->Build.NotifyDataChanged(Revision, Revision);
			CancelFineShardAdmission(Key);
		}
	});
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
	auto* Slot = FineShardSlots.Find(InKey);
	if (!Slot) return;
	FFineShardSlot Retiring = MoveTemp(*Slot);
	FineShardSlots.Remove(InKey);
	if (Retiring.PresentationQueueIndex != INDEX_NONE)
		FinePresentationQueues[Retiring.PresentationQueueBand].RemoveAt(Retiring.PresentationQueueIndex);
	FineShardSlotOrder.RemoveAt(Retiring.OrderIndex);
	const int64 Axis = InKey.SideCells / 16;
	check(FineShardOwnershipAtomCapacity >= Axis * Axis * Axis);
	FineShardOwnershipAtomCapacity -= Axis * Axis * Axis;
	ClearFineShardAdmission(Retiring);
	const auto Stamp = Retiring.SubmittedStamp;
	Scheduler.CancelMatching([Stamp](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Candidate)
	{
		return Kind == EVoxelTaskKind::BuildFineShard && Candidate == Stamp;
	});
	Module.ReleaseGenerationFacts(Retiring.Consumer);
}

void FVoxelViewManager::AdvanceNativeFineRetirement()
{
	const double Deadline = TerrainDeadline(0.05);
	for (int32 Count = 0; Count < 8 && FPlatformTime::Seconds() < Deadline; ++Count)
	{
		if (Module.GetRuntime()->GetChangeHierarchy().AdvanceFineReleases(1) == 0) break;
	}
	const auto Snapshot = AppliedInterestSnapshot;
	if (!Snapshot || FineShardSlotOrder.IsEmpty()) return;
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
	const auto Priority = Demand->ServicePriority;
	const FVoxelFineShardKey Key{InSection * 16, 16};
	FFineShardSlot* Slot = FineShardSlots.Find(Key);
	if (!Slot)
	{
		FString Error;
		if (FineShardSlots.Num() == MAX_int32 || !ReserveFineShardSlots(FineShardSlots.Num() + 1, Error)) return false;
		FVoxelFineMeshInput Input;
		Input.WorldEpoch = WorldEpoch;
		check(NextFineShardIncarnation < MAX_uint64);
		Input.Incarnation = ++NextFineShardIncarnation;
		Input.GeneratorIdentity = Module.GetManifest().BaseSampleHash;
		Input.RegistryRevision = Registry->Hash;
		Input.ShapeRevision = Shapes->GetContentHash();
		Input.MeshRevision = 1;
		Input.Shard = Key;
		Input.DataRevision = Input.OverlayRevision = Runtime->GetChangeHierarchy().GetFineShardRevision(Key);
		FFineShardSlot Created;
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
		const auto Input = Slot->Build.GetInput();
		CancelFineShardAdmission(Key);
		Slot = FineShardSlots.Find(Key);
		if (!Slot || Slot->Build.GetInput() != Input) return false;
	}
	if (!EnsureFineShardRegistration(Key)) return false;
	if (Slot->bSubmitted || Slot->Build.IsMeshReady() || !Slot->Build.GetFailure().IsEmpty() ||
		IsFineShardAdmissionWaiting(*Slot)) return false;
	const auto Attempt = Slot->Build.BeginAttempt();
	Slot->Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, FineShardOwnerId,
		Attempt.Input.Incarnation, InSection, Attempt.Serial);
	Slot->SubmittedStamp.WorldEpoch = WorldEpoch;
	Slot->SubmittedStamp.OwnerId = FineShardOwnerId;
	Slot->SubmittedStamp.OwnerIncarnation = Attempt.Input.Incarnation;
	Slot->SubmittedStamp.Token = Attempt.Serial;
	Slot->SubmittedStamp.Revision = Attempt.Input.MeshRevision;
	Slot->SubmittedStamp.Section = InSection;
	if (Slot->PendingRequest.IsSet()) return SubmitFineShardAdmission(Key, Attempt);
	const uint64 InputBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineShardSamples>() +
		VoxelBoundedAllocation::InitialGrowBytes<uint32>(4096) + 6 * VoxelBoundedAllocation::InitialGrowBytes<uint32>(256);
	const uint64 WorkerBytes = VoxelBoundedAllocation::WorkerSharedBytes<FF0FineWorker>() +
		FMemory::QuantizeSize(sizeof(FVoxelFineShardMeshing), alignof(FVoxelFineShardMeshing));
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Consumer = Slot->Consumer;
	if (!Slot->PendingF0Worker)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease, WorkerLease;
		const uint64 BeforeCapture = Resources->ReleaseVersionExcludingOwner(Consumer);
		auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, InputBytes, Lease, 0, 0, 1);
		if (Admission == EVoxelTerrainResourceAdmission::Granted)
			Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, WorkerBytes, WorkerLease, 0, Lease->Bytes(), 2);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked) WaitFineShardAdmission(*Slot, Resources, BeforeCapture);
			else return RejectFineShardAdmission(Key, Attempt, Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
				TEXT("Oversized: F0 worker input is unreachable") : TEXT("InvalidRequest: F0 worker input domain"), BeforeCapture);
			return false;
		}
		auto Samples = VoxelBoundedAllocation::MakeWorkerShared<FVoxelFineShardSamples>(Lease);
		Samples->ResourceLease = MoveTemp(Lease);
		Samples->RegistrationLease = Slot->RegistrationLease;
		Samples->Attempt = Attempt;
		auto Worker = VoxelBoundedAllocation::MakeWorkerShared<FF0FineWorker>(WorkerLease, Resources, Registry.ToSharedRef(), Shapes.ToSharedRef());
		Worker->WorkerLease = MoveTemp(WorkerLease);
		Worker->Samples = MoveTemp(Samples);
		Worker->Consumer = Consumer;
		Worker->Policy = Module.GetViewSettings().Fine;
		Worker->SectionStamp = Section->Stamp;
		Worker->Revision = InRevision;
		Slot->PendingF0Worker = MoveTemp(Worker);
	}
	const auto Worker = Slot->PendingF0Worker;
	if (Worker->SectionStamp != Section->Stamp || Worker->Revision != InRevision)
	{
		// Runtime 实例被释放重建；结束该固定捕获，不把旧 Snapshot 重绑新实例。
		CancelFineShardAdmission(Key);
		return false;
	}
	const auto Samples = ConstCastSharedPtr<FVoxelFineShardSamples>(Worker->Samples);
	if (!Module.IsAuthority() && !Samples->OverlayVersion)
	{
		FVoxelGenerationBounds Bounds;
		FString Error;
		const auto* Replica = Module.GetOverlayReplica();
		if (!Replica || !CaptureBounds(Key, Bounds)) return false;
		const uint64 BeforeVersion = Scheduler.GetRuntimeResources()->ReleaseVersionExcludingOwner(Consumer);
		if (!Replica->CaptureVersion(Bounds, Samples->OverlayVersion, Error, Consumer))
		{
			if (Error.StartsWith(TEXT("CapacityBlocked")))
				WaitFineShardAdmission(*Slot, Scheduler.GetRuntimeResources(), BeforeVersion);
			else if (!Error.StartsWith(TEXT("WaitingOverlay"))) return RejectFineShardAdmission(Key, Attempt, Error, BeforeVersion);
			return false;
		}
	}
	if (!Worker->bSnapshotCaptured)
	{
		if (!Runtime->CaptureSnapshot(InSection, Samples->Snapshot, Samples->ResourceLease)) return false;
		Worker->bSnapshotCaptured = true;
	}
	while (Worker->NextHalo < 6)
	{
		const uint8 Face = Worker->NextHalo;
		if (Samples->Snapshot.Known[Face]) { ++Worker->NextHalo; continue; }
		const int64 NeighborMinZ = (int64(InSection.Z) + (Face == 4 ? 1 : Face == 5 ? -1 : 0)) * 16;
		if (NeighborMinZ >= Module.GetManifest().Settings.MaxZ || NeighborMinZ + 16 <= Module.GetManifest().Settings.MinZ)
		{
			Samples->Snapshot.Known[Face] = true;
			for (uint32& Packed : Samples->Snapshot.Halo[Face]) Packed = FVoxelBlockState().Pack();
		}
		else if (!Runtime->CompleteSnapshotHalo(Samples->Snapshot, Face)) return false;
		++Worker->NextHalo;
		return false;
	}
	Slot->PendingRequest.Emplace();
	auto& Request = Slot->PendingRequest.GetValue();
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
	Request.ServiceBand = Priority.Band;
	Request.SourcePriority = Priority.SourcePriority;
	Request.SourceStableKey = Priority.SourceStableKey;
	Request.DistanceScore = Priority.DistanceCells;
	Request.ForwardScore = Priority.ForwardScore;
	Request.WorkClass = Request.ServiceBand <= EVoxelTerrainServiceBand::P1 ? EVoxelWorkClass::Critical : EVoxelWorkClass::Interactive;
	Request.FirstRunnableSeconds = Slot->FirstQueuedSeconds;
	Request.InputBytes = InputBytes + WorkerBytes;
	Request.ReservedBytes = sizeof(FVoxelFineShardGeometry);
	Request.bResumable = true;
	Request.GenerationConsumer = Worker->Consumer;
	Request.SetRequiredBindingInput(Samples->ResourceLease->Bytes() + Worker->WorkerLease->Bytes(), 3);
	return SubmitFineShardAdmission(Key, Attempt);
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
		if (Slot->RenderGeometry == Slot->Build.GetPreparedGeometry())
		{
			if (Slot->RenderAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked)
			{
				uint64 Version = 0;
				const auto BootstrapAdmission = Renderer.GetNativeStateAdmission();
				if (Slot->bWaitingRenderBootstrap && (BootstrapAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ||
					BootstrapAdmission == EVoxelTerrainResourceAdmission::InvalidRequest))
				{
					Slot->RenderAdmission = BootstrapAdmission;
					Slot->bWaitingRenderBootstrap = false;
					continue;
				}
				const bool bBootstrapReady = Slot->bWaitingRenderBootstrap && BootstrapAdmission == EVoxelTerrainResourceAdmission::Granted;
				if (!bBootstrapReady && (!Scheduler.GetTerrainResources()->TryGetReleaseVersionExcludingOwner(Slot->Consumer, Version) ||
					Version <= Slot->RenderCapacityVersion)) continue;
				Slot->RenderAdmission = EVoxelTerrainResourceAdmission::Granted;
				Slot->bWaitingRenderBootstrap = false;
			}
			else if (Slot->RenderAdmission != EVoxelTerrainResourceAdmission::Granted) continue;
		}
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
	const auto ObserveRenderAdmission = [&](const EVoxelTerrainResourceAdmission Admission, const uint64 Before)
	{
		Slot.RenderAdmission = Admission;
		Slot.RenderCapacityVersion = Before;
		Slot.bWaitingRenderBootstrap = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked &&
			Renderer.GetNativeStateAdmission() == EVoxelTerrainResourceAdmission::CapacityBlocked;
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
			Scheduler.ObserveDemand(EVoxelTaskKind::BuildFineShard, Slot.SubmittedStamp,
				Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? EVoxelTaskBlockReason::Memory :
				Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? EVoxelTaskBlockReason::Oversized :
				EVoxelTaskBlockReason::TerminalFailure);
	};
	const auto MakeAtom = [&](const FIntVector& Section) -> TSharedPtr<FVoxelTerrainRenderAtom, ESPMode::ThreadSafe>
	{
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTerrainRenderAtom>() +
			VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelFineBoundarySnapshot, ESPMode::ThreadSafe>>(1);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		const uint64 Before = Resources->ReleaseVersionExcludingOwner(Slot.Consumer);
		const auto Admission = Resources->TryAcquire(Slot.Consumer, EVoxelTerrainResourcePool::Staged, Bytes, Lease,
			0, FineShardBackingLease ? FineShardBackingLease->Bytes() : 0, FineShardBackingLease ? 1 : 0);
		ObserveRenderAdmission(Admission, Before);
		if (Admission != EVoxelTerrainResourceAdmission::Granted) return nullptr;
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
			// 每微步撤销一个未封存引用，原 Atom 的预授予 deleter 负责真实后台释放。
			if (!Slot.RenderAtoms.IsEmpty())
			{
				Slot.RenderAtoms.Pop(EAllowShrinking::No);
				return;
			}
			Slot.RenderGeometry = Geometry;
			for (auto& Transaction : Slot.RenderTransactions) Transaction.Invalidate();
			Slot.RenderAdmission = EVoxelTerrainResourceAdmission::Granted;
			Slot.RenderCapacityVersion = 0;
			Slot.bWaitingRenderBootstrap = false;
			Slot.RenderPieceCursor = Slot.RenderBatchCursor = Slot.RenderAtomCursor = 0;
			Slot.RenderTriangleCursor = 0;
			Slot.RenderLeaseCursor = -2;
			Slot.bRenderPrepared = false;
			Slot.bPresentationComplete = Slot.bCycleNeedsPresentation = false;
		}
		const int32 AtomSide = Key.SideCells / 16;
		const int32 AtomCount = AtomSide * AtomSide * AtomSide;
		check(AtomCount > 0 && AtomCount <= 64);
		if (Slot.RenderAtoms.Num() < AtomCount)
		{
			const int32 AtomIndex = Slot.RenderAtoms.Num();
			const FIntVector Section = Key.Min / 16 + FIntVector(AtomIndex % AtomSide,
				(AtomIndex / AtomSide) % AtomSide, AtomIndex / (AtomSide * AtomSide));
			if (auto Atom = MakeAtom(Section)) Slot.RenderAtoms.Add(MoveTemp(Atom));
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
			const FIntVector LocalSection = Section - Key.Min / 16;
			if (LocalSection.X < 0 || LocalSection.Y < 0 || LocalSection.Z < 0 ||
				LocalSection.X >= AtomSide || LocalSection.Y >= AtomSide || LocalSection.Z >= AtomSide) return;
			const int32 PieceAtomIndex = LocalSection.X + AtomSide * (LocalSection.Y + AtomSide * LocalSection.Z);
			const auto Atom = Slot.RenderAtoms[PieceAtomIndex];
			if (!Atom) return;
			const uint64 OldRefBytes = Atom->Chunks.GetAllocatedSize();
			const uint64 RefBytes = FMath::Max<uint64>(OldRefBytes,
				VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelTerrainRenderChunk, ESPMode::ThreadSafe>>(Atom->Chunks.Num() + 1));
			const uint64 PeakBytes = Atom->GetAllocatedBytes() + (RefBytes > OldRefBytes ? RefBytes : 0);
			const uint64 Before = Resources->ReleaseVersionExcludingOwner(Slot.Consumer);
			const auto Admission = Atom->MetadataLease->TryResize(PeakBytes,
				FineShardBackingLease ? FineShardBackingLease->Bytes() : 0, FineShardBackingLease ? 1 : 0);
			ObserveRenderAdmission(Admission, Before);
			if (Admission != EVoxelTerrainResourceAdmission::Granted) return;
			Atom->Chunks.Reserve(Atom->Chunks.Num() + 1);
			check(Atom->Chunks.GetAllocatedSize() <= RefBytes);
			verify(Atom->MetadataLease->Resize(Atom->GetAllocatedBytes()));
			const int32 TriangleCount = Piece.Mesh->Batches[Slot.RenderBatchCursor].Mesh.Triangles.Num() / 3;
			const int32 UploadCount = FMath::Min(4096, TriangleCount - Slot.RenderTriangleCursor);
			EVoxelTerrainResourceAdmission ChunkAdmission = EVoxelTerrainResourceAdmission::None;
			const uint64 BeforeChunk = Resources->ReleaseVersionExcludingOwner(Slot.Consumer);
			const auto Chunk = Renderer.PrepareChunk(Slot.Consumer, Piece.Mesh, Slot.RenderBatchCursor,
				FVector(Piece.Key.Min) * Module.BlockSize(), Module.BlockSize(), Slot.RenderTriangleCursor, UploadCount, &ChunkAdmission);
			if (!Chunk)
			{
				ObserveRenderAdmission(ChunkAdmission, BeforeChunk);
				return;
			}
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
			if (Lease)
			{
				const uint64 Before = Resources->ReleaseVersionExcludingOwner(Slot.Consumer);
				if (!Lease->IsOwnedBy(Resources.Get()))
				{
					ObserveRenderAdmission(EVoxelTerrainResourceAdmission::InvalidRequest, Before);
					return;
				}
				const bool bAllowOverlap = Renderer.HasPresentedCoverage(FBox(FVector(Key.Bounds().Min), FVector(Key.Bounds().Max)));
				if (!Lease->Transfer(EVoxelTerrainResourcePool::Resident) &&
					(!bAllowOverlap || !Lease->Transfer(EVoxelTerrainResourcePool::ResidentOverlap)))
				{
					const auto Limits = Resources->Snapshot().Limits;
					ObserveRenderAdmission(Lease->Bytes() > Limits.ResidentBytes && (!bAllowOverlap || Lease->Bytes() > Limits.OverlapBytes) ?
						EVoxelTerrainResourceAdmission::CapacityUnachievable : EVoxelTerrainResourceAdmission::CapacityBlocked, Before);
					return;
				}
			}
			++Slot.RenderLeaseCursor;
			return;
		}
		Slot.RenderAtomCursor %= AtomCount;
		const int32 AtomIndex = Slot.RenderAtomCursor++;
		auto& PendingTransaction = Slot.RenderTransactions[AtomIndex];
		if (PendingTransaction.IsValid())
		{
			if (Renderer.ConsumeTransactionFailure(PendingTransaction))
			{
				PendingTransaction.Invalidate();
				Slot.RenderAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
				return;
			}
			if (Renderer.IsTransactionPending(PendingTransaction))
			{
				Slot.bCycleNeedsPresentation = true;
				return;
			}
			PendingTransaction.Invalidate();
		}
		if (AtomIndex == 0) Slot.bCycleNeedsPresentation = false;
		const FIntVector Section = Key.Min / 16 + FIntVector(AtomIndex % AtomSide,
			(AtomIndex / AtomSide) % AtomSide, AtomIndex / (AtomSide * AtomSide));
		if (Key.SideCells > 16 && GetInputRoot().FineWanted.Contains(Section) && IsFineGeometryPrepared(Section))
		{
			if (Slot.RenderAtomCursor == AtomCount && !Slot.bCycleNeedsPresentation) Slot.bPresentationComplete = true;
			return;
		}
		const auto Atom = Slot.RenderAtoms[AtomIndex];
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
		EVoxelTerrainResourceAdmission TransactionAdmission = EVoxelTerrainResourceAdmission::Granted;
		const uint64 BeforeTransaction = Scheduler.GetTerrainResources()->ReleaseVersionExcludingOwner(Slot.Consumer);
		if (Renderer.PrepareAtomTransaction(MakeArrayView(&ImmutableAtom, 1), AppliedInterestRevision, bRequiresCollision,
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
			[this, Section] { OnPresentationReady(Section); }, Transaction, Candidates[Chosen].Priority, Slot.FirstQueuedSeconds,
			&TransactionAdmission, Slot.Consumer)) PendingTransaction = Transaction;
		else if (TransactionAdmission != EVoxelTerrainResourceAdmission::Granted)
			ObserveRenderAdmission(TransactionAdmission, BeforeTransaction);
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
