#include "Voxel/Generation/VoxelGenerationPlanCoordinator.h"

#include "HAL/PlatformTime.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

void FVoxelGenerationPlanCoordinator::AttachPendingRequest(FState& InState,
	TSharedPtr<FPendingRequestStorage, ESPMode::ThreadSafe> InRecord)
{
	check(Storage && !InState.PendingRequest.IsSet() && InRecord && InRecord->ResourceLease);
	++Storage->PendingRequestCount;
	Storage->PendingRequestBytes += InRecord->ResourceLease->Bytes();
	InState.PendingRequest.Record = MoveTemp(InRecord);
}

TSharedPtr<FVoxelGenerationPlanCoordinator::FPendingRequestStorage, ESPMode::ThreadSafe>
FVoxelGenerationPlanCoordinator::DetachPendingRequest(FState& InState)
{
	if (!InState.PendingRequest.IsSet()) return nullptr;
	check(Storage && Storage->PendingRequestCount > 0 &&
		Storage->PendingRequestBytes >= InState.PendingRequest.Record->ResourceLease->Bytes());
	--Storage->PendingRequestCount;
	Storage->PendingRequestBytes -= InState.PendingRequest.Record->ResourceLease->Bytes();
	return MoveTemp(InState.PendingRequest.Record);
}

bool FVoxelGenerationPlanCoordinator::PreparePendingRequest(const FVoxelGenerationPlanDependency& InDependency, FState& InState)
{
	if (InState.PendingRequest.IsSet()) return true;
	check(!InState.PendingReadDependencies);
	if (!InState.bAttemptPendingAdmission)
	{
		checkf(NextAttemptId != MAX_uint64, TEXT("Generation attempt serial exhausted"));
		if (InState.AttemptId && (NextAttemptId + 1) % 1024 == 0)
			UE_LOG(LogTemp, Display, TEXT("Voxel fact new attempt: kind=%d key=%s previous=%llu next=%llu state=%d inFlight=%d cached=%d prerequisites=%d subscriptions=%d parents=%d"),
				int32(InDependency.Kind), *InDependency.Coordinate().ToString(), InState.AttemptId, NextAttemptId + 1,
				int32(InState.State), InState.bInFlight, IsCached(InDependency), InState.Prerequisites.Num(),
				InState.Donations.Num(), PriorityGraph.HasConsumers(InState.PriorityNode));
		InState.AttemptId = ++NextAttemptId;
		InState.AttemptFirstRunnableSeconds = FPlatformTime::Seconds();
		InState.bAttemptPendingAdmission = true;
	}
	const auto Stamp = MakeTaskStamp(InDependency, InState.AttemptId);
	const FVoxelGenerationConsumerKey Consumer{EVoxelGenerationConsumerKind::WorldFact, WorldEpoch,
		EnvironmentOwnerId, EnvironmentOwnerIncarnation, EVoxelGenerationSpatialKind::Section,
		Stamp.Section, Stamp.Section, Stamp.Token};
	const uint64 Before = Resources->ReleaseVersionExcludingOwner(Consumer);
	if (InState.bWaitingCapacity && Before <= InState.CapacityVersion)
	{
		LinkPendingAdmission(InState);
		return false;
	}
	FVoxelGenerationRequiredSourceScope RootSources(&Resources.Get(), this, &VisitControlSources);
	FVoxelGenerationRequiredSourceScope StateSources(&Resources.Get(), &InState,
		[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& State = *static_cast<const FState*>(Context);
			for (const auto* Lease : {State.PrerequisitesLease.Get(), State.DonationsLease.Get()})
				if (Lease) FVoxelGenerationRequiredSourceScope::VisitLease(Lease, Visit);
			if (State.PrerequisiteRetention)
				FVoxelGenerationRequiredSourceScope::VisitLease(&State.PrerequisiteRetention->GetResourceLease().Get(), Visit);
		});
	FVoxelGenerationRequiredCapacity Required;
	FString Error;
	if (!FVoxelGenerationRequiredSourceScope::Collect(Resources.Get(), nullptr, Required, Error))
	{
		FailAdmission(InDependency, InState, Error, EVoxelTaskBlockReason::Oversized);
		return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FPendingRequestStorage>(), Lease, 0,
		Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked)
		{
			InState.bWaitingCapacity = true;
			InState.CapacityVersion = Before;
			LinkPendingAdmission(InState);
			Scheduler.ObserveDemand(EVoxelTaskKind::BuildGenerationPlan, Stamp, EVoxelTaskBlockReason::InputMemory);
		}
		else FailAdmission(InDependency, InState, Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
			TEXT("Oversized: generation pending request and retained inputs") : TEXT("InvalidRequest: generation pending request"),
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? EVoxelTaskBlockReason::Oversized : EVoxelTaskBlockReason::TerminalFailure);
		return false;
	}
	InState.bWaitingCapacity = false;
	auto Record = VoxelBoundedAllocation::MakeWorkerShared<FPendingRequestStorage>(Lease);
	Record->ResourceLease = MoveTemp(Lease);
	Record->Request.Kind = EVoxelTaskKind::BuildGenerationPlan;
	Record->Request.TerrainStage = 0;
	Record->Request.Stamp = Stamp;
	Record->Request.GenerationConsumer = Consumer;
	Record->Request.FirstRunnableSeconds = InState.AttemptFirstRunnableSeconds;
	Record->Request.ReservedBytes = 0;
	AttachPendingRequest(InState, MoveTemp(Record));
	return true;
}
