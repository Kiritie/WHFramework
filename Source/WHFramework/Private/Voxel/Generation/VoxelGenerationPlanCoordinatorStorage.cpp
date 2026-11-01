#include "Voxel/Generation/VoxelGenerationPlanCoordinator.h"

#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

void FVoxelGenerationPlanCoordinator::LogStorageDiagnostics(const double InNow)
{
#if !UE_BUILD_SHIPPING
	if (InNow < NextStorageDiagnosticSeconds || !Storage || Storage->Owner != this) return;
	NextStorageDiagnosticSeconds = InNow + 5.0;
	const auto* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("wh.Voxel.DebugRepresentation"));
	if (!Debug || Debug->GetInt() <= 0) return;
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_PlanStorageDiagnostics);
	UE_LOG(LogTemp, Display, TEXT("Voxel plan control types: owner=%s dependencyBytes=%llu stateBytes=%llu consumerBytes=%llu subscriptionBytes=%llu"),
		*ControlConsumer.OwnerId.ToString(), uint64(sizeof(FVoxelGenerationPlanDependency)), uint64(sizeof(FState)),
		uint64(sizeof(FConsumerState)), uint64(sizeof(FSubscription)));
	UE_LOG(LogTemp, Display, TEXT("Voxel plan pending request storage: owner=%s attached=%d grantedBytes=%llu recordBytes=%llu requestBytes=%llu admissionCount=%d requestOwner=%s incarnation=%llu"),
		*ControlConsumer.OwnerId.ToString(), Storage->PendingRequestCount, Storage->PendingRequestBytes,
		VoxelBoundedAllocation::WorkerSharedBytes<FPendingRequestStorage>(), uint64(sizeof(FVoxelTaskRequest)), Storage->AdmissionCount,
		*EnvironmentOwnerId.ToString(), EnvironmentOwnerIncarnation);
	UE_LOG(LogTemp, Display, TEXT("Voxel plan donation aggregation: owner=%s mutations=%llu rescans=%llu verified=%llu"),
		*ControlConsumer.OwnerId.ToString(), Storage->DonationMutations, Storage->DonationRescans, Storage->DonationVerifications);
	const auto LogMap = [this](const TCHAR* Name, const auto& Map, const int32 Capacity, const auto& Lease)
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel plan control storage: owner=%s epoch=%llu incarnation=%llu serial=%llu name=%s entries=%d capacity=%d maxIndex=%d nativeBytes=%llu lease=%llu granted=%llu"),
			*ControlConsumer.OwnerId.ToString(), ControlConsumer.WorldEpoch, ControlConsumer.OwnerIncarnation,
			ControlConsumer.RequestSerial, Name, Map.Num(), Capacity, Map.GetMaxIndex(),
			VoxelGenerationAllocation::AllocatedBytes(Map), Lease ? Lease->Serial() : 0, Lease ? Lease->Bytes() : 0);
	};
	LogMap(TEXT("States"), Storage->States, Storage->StatesCapacity, Storage->StatesLease);
	LogMap(TEXT("Consumers"), Storage->Consumers, Storage->ConsumersCapacity, Storage->ConsumersLease);
	LogMap(TEXT("PriorityFacts"), Storage->PriorityFacts, Storage->FactsCapacity, Storage->FactsLease);
	LogMap(TEXT("EnvironmentAttempts"), Storage->EnvironmentAttempts, Storage->EnvironmentCapacity, Storage->EnvironmentLease);
	PriorityGraph.LogStorageDiagnostics();
#endif
}
