#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Geometry/VoxelCollisionBuilder.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelDetailView.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Save/VoxelDeltaTypes.h"

bool FVoxelTaskStamp::operator==(const FVoxelTaskStamp& InOther) const
{
	return WorldEpoch == InOther.WorldEpoch &&
		Token == InOther.Token &&
		Revision == InOther.Revision &&
		Section == InOther.Section &&
		ViewKey == InOther.ViewKey &&
		SurfaceKey == InOther.SurfaceKey &&
		MacroKey == InOther.MacroKey;
}

uint64 FVoxelTaskResult::ResultBytes() const
{
	uint64 Bytes = static_cast<uint64>(BaseBlocks.Num()) * sizeof(FVoxelBlockState) + Payload.Num();
	Bytes += Collision ? Collision->Bytes() : 0;
	Bytes += FineMesh ? FineMesh->Bytes() : 0;
	Bytes += VoxelProxyMesh ? VoxelProxyMesh->Bytes() : 0;
	Bytes += SurfaceMesh ? SurfaceMesh->Bytes() : 0;
	Bytes += WaterMesh ? WaterMesh->Bytes() : 0;
	Bytes += MacroMesh ? MacroMesh->Bytes() : 0;
	Bytes += VoxelProxy ? static_cast<uint64>(VoxelProxy->Cells.Num()) * sizeof(FVoxelBlockState) : 0;
	if (Surface)
	{
		Bytes += static_cast<uint64>(Surface->GroundZ.Num()) * sizeof(int32);
		Bytes += static_cast<uint64>(Surface->WaterZ.Num()) * sizeof(int32);
		Bytes += static_cast<uint64>(Surface->SurfaceMaterial.Num()) * sizeof(uint16);
		Bytes += static_cast<uint64>(Surface->Biome.Num()) * sizeof(uint16);
		Bytes += Surface->Flags.Num();
	}
	if (Water)
	{
		Bytes += static_cast<uint64>(Water->WaterZ.Num()) * sizeof(int32);
		Bytes += Water->WaterKind.Num();
	}
	if (Macro)
	{
		Bytes += static_cast<uint64>(Macro->Height.Num()) * sizeof(int32);
		Bytes += static_cast<uint64>(Macro->WaterHeight.Num()) * sizeof(int32);
		Bytes += static_cast<uint64>(Macro->SurfaceClass.Num()) * sizeof(uint16);
		Bytes += Macro->ForestCoverage.Num();
		Bytes += Macro->SnowCoverage.Num();
		Bytes += static_cast<uint64>(Macro->LargeStructures.Num()) * sizeof(FVoxelMacroStructureProxy);
	}
	Bytes += Details ? Details->GetAllocatedBytes() : 0;
	if (PersistentSection)
	{
		Bytes += static_cast<uint64>(PersistentSection->Blocks.Num()) * (sizeof(int32) + sizeof(FVoxelBlockState));
		for (const TPair<int32, FVoxelBlockEntityState>& Pair : PersistentSection->Entities)
		{
			Bytes += sizeof(int32) + sizeof(FVoxelBlockEntityState) + Pair.Value.Payload.Num();
		}
	}
	return Bytes;
}

FVoxelTaskScheduler::FVoxelTaskScheduler()
{
	Budget.MaxConcurrentTasks = FMath::Clamp(
		FPlatformMisc::NumberOfWorkerThreadsToSpawn() / 2,
		1,
		8);
}

FVoxelTaskScheduler::~FVoxelTaskScheduler()
{
	StopAndJoin();
}

bool FVoxelTaskScheduler::Enqueue(FVoxelTaskRequest&& InRequest)
{
	check(IsInGameThread());
	if (bStopped ||
		!InRequest.Execute ||
		InRequest.Kind == EVoxelTaskKind::None ||
		InRequest.WorkClass == EVoxelWorkClass::None ||
		!FMath::IsFinite(InRequest.DistanceScore) ||
		!FMath::IsFinite(InRequest.ForwardScore) ||
		InRequest.ReservedBytes == 0 ||
		InRequest.ReservedBytes > Budget.MaxReservedBytes ||
		InRequest.InputBytes > Budget.MaxInputBytes)
	{
		return false;
	}
	if (Has(InRequest.Stamp, InRequest.Kind))
	{
		return true;
	}
	if (Pending.Num() >= 1024 || InRequest.InputBytes > Budget.MaxInputBytes - QueuedInputBytes)
	{
		return false;
	}

	InRequest.QueuedAt = FPlatformTime::Seconds();
	QueuedInputBytes += InRequest.InputBytes;
	Pending.Add(MoveTemp(InRequest));
	Pump();
	return true;
}

void FVoxelTaskScheduler::Tick(
	TFunctionRef<void(FVoxelTaskResult&&)> InApply,
	const double InMaxApplyMilliseconds)
{
	check(IsInGameThread());
	if (bStopped)
	{
		return;
	}

	const double EndTime = FPlatformTime::Seconds() + FMath::Max(0.0, InMaxApplyMilliseconds) / 1000.0;
	int32 Applied = 0;
	for (int32 Index = 0;
		Index < Running.Num() &&
		Applied < Budget.MaxCompletedResultsPerFrame &&
		FPlatformTime::Seconds() <= EndTime;)
	{
		if (!Running[Index].Task.IsCompleted())
		{
			++Index;
			continue;
		}

		FRunning Completed = MoveTemp(Running[Index]);
		Running.RemoveAtSwap(Index);
		ReservedBytes -= Completed.ReservedBytes;
		if (Completed.Apply)
		{
			Completed.Apply(MoveTemp(Completed.Slot->Result));
		}
		else
		{
			InApply(MoveTemp(Completed.Slot->Result));
		}
		++Applied;
	}

	while (!Canceled.IsEmpty() &&
		Applied < Budget.MaxCompletedResultsPerFrame &&
		FPlatformTime::Seconds() <= EndTime)
	{
		FCompleted Completed = MoveTemp(Canceled.Last());
		Canceled.Pop(EAllowShrinking::No);
		if (Completed.Apply)
		{
			Completed.Apply(MoveTemp(Completed.Result));
		}
		else
		{
			InApply(MoveTemp(Completed.Result));
		}
		++Applied;
	}
	Pump();
}

void FVoxelTaskScheduler::CancelSection(const FIntVector& InSection)
{
	check(IsInGameThread());
	for (int32 Index = Pending.Num() - 1; Index >= 0; --Index)
	{
		if (Pending[Index].Stamp.Section == InSection)
		{
			QueuedInputBytes -= Pending[Index].InputBytes;
			FVoxelTaskRequest Request = MoveTemp(Pending[Index]);
			Pending.RemoveAtSwap(Index);
			QueueCanceled(MoveTemp(Request));
		}
	}
	for (FRunning& Task : Running)
	{
		if (Task.Stamp.Section == InSection)
		{
			Task.Slot->Cancel.Store(true);
		}
	}
}

void FVoxelTaskScheduler::StopAndJoin()
{
	if (bStopped && Running.IsEmpty())
	{
		return;
	}
	bStopped = true;
	Pending.Reset();
	Canceled.Reset();
	QueuedInputBytes = 0;
	for (FRunning& Task : Running)
	{
		Task.Slot->Cancel.Store(true);
	}
	for (FRunning& Task : Running)
	{
		Task.Task.Wait();
	}
	Running.Reset();
	ReservedBytes = 0;
}

bool FVoxelTaskScheduler::Has(
	const FVoxelTaskStamp& InStamp,
	const EVoxelTaskKind InKind) const
{
	for (const FVoxelTaskRequest& Request : Pending)
	{
		if (Request.Kind == InKind && Request.Stamp == InStamp)
		{
			return true;
		}
	}
	for (const FRunning& Task : Running)
	{
		if (Task.Kind == InKind &&
			Task.Stamp == InStamp &&
			!Task.Slot->Cancel.Load())
		{
			return true;
		}
	}
	return false;
}

bool FVoxelTaskScheduler::HasSectionTask(const FIntVector& InSection) const
{
	for (const FVoxelTaskRequest& Request : Pending)
	{
		if (Request.Stamp.Section == InSection)
		{
			return true;
		}
	}
	for (const FRunning& Task : Running)
	{
		if (Task.Stamp.Section == InSection && !Task.Slot->Cancel.Load())
		{
			return true;
		}
	}
	return false;
}

int32 FVoxelTaskScheduler::ActiveCount() const
{
	return Running.Num();
}

int32 FVoxelTaskScheduler::CriticalCount() const
{
	int32 Count = 0;
	for (const FVoxelTaskRequest& Request : Pending)
	{
		Count += Request.WorkClass == EVoxelWorkClass::Critical ? 1 : 0;
	}
	for (const FRunning& Task : Running)
	{
		Count += Task.WorkClass == EVoxelWorkClass::Critical && !Task.Slot->Cancel.Load() ? 1 : 0;
	}
	return Count;
}

void FVoxelTaskScheduler::SetBudget(const FVoxelTaskBudget& InBudget)
{
	check(IsInGameThread());
	Budget.MaxConcurrentTasks = FMath::Max(1, InBudget.MaxConcurrentTasks);
	Budget.MaxReservedBytes = FMath::Max<uint64>(1, InBudget.MaxReservedBytes);
	Budget.MaxInputBytes = FMath::Max<uint64>(1, InBudget.MaxInputBytes);
	Budget.MaxCompletedResultsPerFrame = FMath::Max(1, InBudget.MaxCompletedResultsPerFrame);
}

bool FVoxelTaskScheduler::IsHigherPriority(
	const FVoxelTaskRequest& InA,
	const FVoxelTaskRequest& InB)
{
	if (InA.WorkClass != InB.WorkClass)
	{
		return static_cast<uint8>(InA.WorkClass) < static_cast<uint8>(InB.WorkClass);
	}
	if (InA.DistanceScore != InB.DistanceScore)
	{
		return InA.DistanceScore < InB.DistanceScore;
	}
	if (InA.ForwardScore != InB.ForwardScore)
	{
		return InA.ForwardScore > InB.ForwardScore;
	}
	return InA.QueuedAt < InB.QueuedAt;
}

void FVoxelTaskScheduler::Pump()
{
	while (!bStopped &&
		Running.Num() < Budget.MaxConcurrentTasks &&
		!Pending.IsEmpty())
	{
		int32 BestIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			const FVoxelTaskRequest& Request = Pending[Index];
			if (Request.ReservedBytes > Budget.MaxReservedBytes - ReservedBytes)
			{
				continue;
			}
			if (BestIndex == INDEX_NONE || IsHigherPriority(Request, Pending[BestIndex]))
			{
				BestIndex = Index;
			}
		}
		if (BestIndex == INDEX_NONE)
		{
			break;
		}

		FVoxelTaskRequest Request = MoveTemp(Pending[BestIndex]);
		Pending.RemoveAtSwap(BestIndex);
		QueuedInputBytes -= Request.InputBytes;
		ReservedBytes += Request.ReservedBytes;

		FRunning RunningTask;
		RunningTask.Stamp = Request.Stamp;
		RunningTask.Kind = Request.Kind;
		RunningTask.WorkClass = Request.WorkClass;
		RunningTask.ReservedBytes = Request.ReservedBytes;
		RunningTask.Slot = MakeShared<FSlot, ESPMode::ThreadSafe>();
		RunningTask.Apply = MoveTemp(Request.Apply);
		const TSharedPtr<FSlot, ESPMode::ThreadSafe> Slot = RunningTask.Slot;
		const FVoxelTaskStamp Stamp = RunningTask.Stamp;
		const EVoxelTaskKind Kind = RunningTask.Kind;
		const uint64 ResultBudget = RunningTask.ReservedBytes;
		RunningTask.Task = UE::Tasks::Launch(
			UE_SOURCE_LOCATION,
			[Slot, Stamp, Kind, ResultBudget, Execute = MoveTemp(Request.Execute)]() mutable
			{
				if (!Slot->Cancel.Load())
				{
					Slot->Result = Execute(Slot->Cancel);
				}
				Slot->Result.Stamp = Stamp;
				Slot->Result.Kind = Kind;
				Slot->Result.bCanceled = Slot->Cancel.Load();
				if (Slot->Result.ResultBytes() > ResultBudget)
				{
					FVoxelTaskResult ErrorResult;
					ErrorResult.Stamp = Stamp;
					ErrorResult.Kind = Kind;
					ErrorResult.Error = TEXT("Voxel task result memory budget exceeded");
					Slot->Result = MoveTemp(ErrorResult);
				}
			});
		Running.Add(MoveTemp(RunningTask));
	}
}

void FVoxelTaskScheduler::QueueCanceled(FVoxelTaskRequest&& InRequest)
{
	FCompleted Completed;
	Completed.Result.Stamp = InRequest.Stamp;
	Completed.Result.Kind = InRequest.Kind;
	Completed.Result.bCanceled = true;
	Completed.Apply = MoveTemp(InRequest.Apply);
	Canceled.Add(MoveTemp(Completed));
}
