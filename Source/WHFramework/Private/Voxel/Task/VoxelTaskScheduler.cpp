#include "Voxel/Task/VoxelTaskScheduler.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Voxel/Geometry/VoxelCollisionBuilder.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/VoxelDetailView.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Save/VoxelDeltaTypes.h"

namespace
{
	bool IsVisualWorkClass(const EVoxelWorkClass InClass)
	{
		switch (InClass)
		{
		case EVoxelWorkClass::Visible:
		case EVoxelWorkClass::Boundary:
		case EVoxelWorkClass::Exploration:
		case EVoxelWorkClass::Background:
			return true;
		default:
			return false;
		}
	}

	int32 TerrainLane(const EVoxelTaskKind Kind)
	{
		switch (Kind)
		{
		case EVoxelTaskKind::BuildFineMesh: return 0;
		case EVoxelTaskKind::BuildVoxelProxy: return 1;
		case EVoxelTaskKind::BuildSurface: return 2;
		case EVoxelTaskKind::BuildMacro: return 3;
		default: return INDEX_NONE;
		}
	}

	bool IsCoarseTerrainKind(const EVoxelTaskKind InKind)
	{
		return
			InKind == EVoxelTaskKind::BuildSurface ||
			InKind == EVoxelTaskKind::BuildMacro;
	}

	bool IsRepresentationDataKind(const EVoxelTaskKind InKind)
	{
		return InKind == EVoxelTaskKind::GenerateVoxelProxy ||
			InKind == EVoxelTaskKind::GenerateSurface || InKind == EVoxelTaskKind::GenerateMacro;
	}

}

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
	uint64 Bytes =
		static_cast<uint64>(BaseBlocks.Num()) * sizeof(FVoxelBlockState) +
		Payload.Num();

	Bytes += Collision ? Collision->Bytes() : 0;
	Bytes += FineMesh ? FineMesh->Bytes() : 0;
	Bytes += VoxelProxyMesh ? VoxelProxyMesh->Bytes() : 0;
	Bytes += SurfaceMesh ? SurfaceMesh->Bytes() : 0;
	Bytes += WaterMesh ? WaterMesh->Bytes() : 0;
	Bytes += MacroMesh ? MacroMesh->Bytes() : 0;

	if (VoxelProxy)
	{
		Bytes += VoxelProxy->GetAllocatedBytes();
	}
	if (Surface)
	{
		Bytes += sizeof(FVoxelSurfaceTileData) + Surface->GroundZ.GetAllocatedSize() +
			Surface->WaterZ.GetAllocatedSize() + Surface->SurfaceMaterial.GetAllocatedSize() +
			Surface->Biome.GetAllocatedSize() + Surface->Flags.GetAllocatedSize() + Surface->DistantCells.GetAllocatedSize();
	}
	if (Water)
	{
		Bytes += sizeof(FVoxelWaterSurfaceTileData) + Water->WaterZ.GetAllocatedSize() + Water->WaterKind.GetAllocatedSize();
	}
	if (Macro)
	{
		Bytes += sizeof(FVoxelMacroTileData) + Macro->Height.GetAllocatedSize() + Macro->WaterHeight.GetAllocatedSize() +
			Macro->SurfaceClass.GetAllocatedSize() + Macro->ForestCoverage.GetAllocatedSize() + Macro->SnowCoverage.GetAllocatedSize() +
			Macro->LargeStructures.GetAllocatedSize() + Macro->DistantCells.GetAllocatedSize();
	}

	Bytes += Details ? Details->GetAllocatedBytes() : 0;

	if (PersistentSection)
	{
		Bytes +=
			static_cast<uint64>(PersistentSection->Blocks.Num()) *
			(sizeof(int32) + sizeof(FVoxelBlockState));

		for (const TPair<int32, FVoxelBlockEntityState>& Pair :
			PersistentSection->Entities)
		{
			Bytes +=
				sizeof(int32) +
				sizeof(FVoxelBlockEntityState) +
				Pair.Value.Payload.Num();
		}
	}

	Bytes +=
		CustomPayload
			? CustomPayload->GetAllocatedBytes()
			: 0;

	return Bytes;
}

FVoxelTaskScheduler::FVoxelTaskScheduler()
{
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

	if (IsCoarseTerrainKind(InRequest.Kind))
	{
		int32 PendingCoarseTerrainTasks = 0;

		for (const FVoxelTaskRequest& PendingRequest : Pending)
		{
			PendingCoarseTerrainTasks +=
				IsCoarseTerrainKind(PendingRequest.Kind)
					? 1
					: 0;
		}

		if (PendingCoarseTerrainTasks >=
			Budget.MaxPendingCoarseTerrainTasks)
		{
			return false;
		}
	}

	InRequest.QueuedAt = FPlatformTime::Seconds();
	const bool bVisual = IsVisualWorkClass(InRequest.WorkClass);
	const int32 Lane = TerrainLane(InRequest.Kind);
	const int32 ReservedPerLane = Budget.MaxPendingTasks >= 16 ? 2 : 0;
	int32 Counts[4] = {};
	for (const FVoxelTaskRequest& Request : Pending)
	{
		const int32 Index = TerrainLane(Request.Kind);
		if (Index != INDEX_NONE) ++Counts[Index];
	}
	int32 PendingLimit = Budget.MaxPendingTasks;
	if (bVisual)
	{
		for (int32 Index = 0; Index < 4; ++Index)
		{
			if (Index != Lane) PendingLimit -= FMath::Max(0, ReservedPerLane - Counts[Index]);
		}
	}
	const bool bReservedAdmission = Lane != INDEX_NONE && Counts[Lane] < ReservedPerLane;
	// 先完整验证替换集合，再取消旧请求，内存不足不能导致部分取消后仍然拒绝新请求。
	TArray<int32, TInlineAllocator<8>> DisplacedIndices;
	uint64 RemainingInput = QueuedInputBytes;
	while (Pending.Num() - DisplacedIndices.Num() >= PendingLimit ||
		InRequest.InputBytes > Budget.MaxInputBytes - RemainingInput)
	{
		int32 Worst = INDEX_NONE;
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			if (DisplacedIndices.Contains(Index)) continue;
			const FVoxelTaskRequest& Existing = Pending[Index];
			const int32 ExistingLane = TerrainLane(Existing.Kind);
			if (bVisual && ExistingLane != INDEX_NONE && ExistingLane != Lane && Counts[ExistingLane] <= ReservedPerLane) continue;
			const bool bMayReplace = IsHigherPriority(InRequest, Existing) ||
				(bReservedAdmission && IsVisualWorkClass(Existing.WorkClass) && ExistingLane != Lane);
			if (bMayReplace && (Worst == INDEX_NONE || IsHigherPriority(Pending[Worst], Existing))) Worst = Index;
		}
		if (Worst == INDEX_NONE) return false;
		DisplacedIndices.Add(Worst);
		RemainingInput -= Pending[Worst].InputBytes;
		const int32 ExistingLane = TerrainLane(Pending[Worst].Kind);
		if (ExistingLane != INDEX_NONE) --Counts[ExistingLane];
	}
	DisplacedIndices.Sort([](const int32 A, const int32 B) { return A > B; });
	for (const int32 Index : DisplacedIndices)
	{
		FVoxelTaskRequest Displaced = MoveTemp(Pending[Index]);
		Pending.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		QueuedInputBytes -= Displaced.InputBytes;
		RemoveActive(Displaced.Stamp, Displaced.Kind, Displaced.WorkClass);
		QueueCanceled(MoveTemp(Displaced));
	}

	QueuedInputBytes +=
		InRequest.InputBytes;

	AddActive(
		InRequest.Stamp,
		InRequest.Kind,
		InRequest.WorkClass);

	Pending.Add(
		MoveTemp(InRequest));

	Pump();
	return true;
}

void FVoxelTaskScheduler::Tick(
	TFunctionRef<void(FVoxelTaskResult&&)> InApply,
	const double InMaxApplyMilliseconds)
{
	check(IsInGameThread());

	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_TaskSchedulerTick);

	if (bStopped)
	{
		return;
	}

	const double EndTime =
		FPlatformTime::Seconds() +
		FMath::Max(
			0.0,
			InMaxApplyMilliseconds) /
			1000.0;

	int32 Applied = 0;
	int32 HeavyApplied = 0;
	int32 FineApplied = 0;
	int32 VoxelLODApplied = 0;
	int32 SurfaceApplied = 0;
	int32 MacroApplied = 0;

	auto CanApplyTerrainKind =
		[this, &FineApplied, &VoxelLODApplied, &SurfaceApplied, &MacroApplied](const EVoxelTaskKind Kind) -> bool
		{
			switch (Kind)
			{
			case EVoxelTaskKind::BuildFineMesh:
				return FineApplied < Budget.MaxFineApplyPerFrame;
			case EVoxelTaskKind::BuildVoxelProxy:
			case EVoxelTaskKind::BuildVolumeTransition:
				return VoxelLODApplied < Budget.MaxVoxelLODApplyPerFrame;
			case EVoxelTaskKind::BuildSurface:
				return SurfaceApplied < Budget.MaxSurfaceApplyPerFrame;
			case EVoxelTaskKind::BuildMacro:
				return MacroApplied < Budget.MaxMacroApplyPerFrame;
			default:
				return true;
			}
		};

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

		if (Running[Index].Slot->Cancel.Load())
		{
			Running[Index].Slot->Result.bCanceled = true;
			Running[Index].Slot->Result.bSuccess = false;
		}
		const bool bHeavy = Running[Index].Slot->Result.HasHeavyApply();

		if (!CanApplyTerrainKind(Running[Index].Kind))
		{
			++Index;
			continue;
		}

		if (bHeavy &&
			HeavyApplied >=
				Budget.MaxHeavyCompletedResultsPerFrame)
		{
			++Index;
			continue;
		}

		FRunning Completed =
			MoveTemp(Running[Index]);

		Running.RemoveAtSwap(Index);

		ReservedBytes -=
			Completed.ReservedBytes;

		RemoveActive(
			Completed.Stamp,
			Completed.Kind,
			Completed.WorkClass);

		if (bHeavy)
		{
			++HeavyApplied;
		}

		switch (Completed.Kind)
		{
		case EVoxelTaskKind::BuildFineMesh:
			++FineApplied;
			break;
		case EVoxelTaskKind::BuildVoxelProxy:
		case EVoxelTaskKind::BuildVolumeTransition:
			++VoxelLODApplied;
			break;
		case EVoxelTaskKind::BuildSurface:
			++SurfaceApplied;
			break;
		case EVoxelTaskKind::BuildMacro:
			++MacroApplied;
			break;
		default:
			break;
		}

		FVoxelTaskResult& Result =
			Completed.Slot->Result;

		/**
		 * Move 前缓存 diagnostics。
		 */
		const bool bResultSuccess =
			Result.bSuccess;

		const bool bResultCanceled =
			Result.bCanceled;

		const double QueueMilliseconds =
			Result.QueueMilliseconds;

		const double ExecuteMilliseconds =
			Result.ExecuteMilliseconds;

		const double ApplyStart =
			FPlatformTime::Seconds();

		if (Completed.Apply)
		{
			Completed.Apply(
				MoveTemp(Result));
		}
		else
		{
			InApply(
				MoveTemp(Result));
		}

		const double ApplyEnd =
			FPlatformTime::Seconds();

		FVoxelTaskResult Sample;

		Sample.Kind =
			Completed.Kind;

		Sample.Stamp =
			Completed.Stamp;

		Sample.bSuccess =
			bResultSuccess;

		Sample.bCanceled =
			bResultCanceled;

		Sample.QueueMilliseconds =
			QueueMilliseconds;

		Sample.ExecuteMilliseconds =
			ExecuteMilliseconds;

		Sample.ApplyMilliseconds =
			(ApplyEnd -
			 ApplyStart) *
			1000.0;

		RecordCompletedResult(
			Sample);

		++Applied;
	}

	while (!Canceled.IsEmpty() &&
		Applied < Budget.MaxCompletedResultsPerFrame &&
		FPlatformTime::Seconds() <= EndTime)
	{
		FCompleted Completed =
			MoveTemp(Canceled.Last());

		Canceled.Pop(
			EAllowShrinking::No);

		FVoxelTaskResult Sample;
		Sample.Kind = Completed.Result.Kind;
		Sample.Stamp = Completed.Result.Stamp;
		Sample.bCanceled = true;

		const double ApplyStart =
			FPlatformTime::Seconds();

		if (Completed.Apply)
		{
			Completed.Apply(
				MoveTemp(Completed.Result));
		}
		else
		{
			InApply(
				MoveTemp(Completed.Result));
		}

		Sample.ApplyMilliseconds =
			(FPlatformTime::Seconds() - ApplyStart) *
			1000.0;

		RecordCompletedResult(Sample);
		++Applied;
	}

	Pump();
}

void FVoxelTaskScheduler::CancelSection(
	const FIntVector& InSection)
{
	check(IsInGameThread());

	for (int32 Index = Pending.Num() - 1;
		Index >= 0;
		--Index)
	{
		if (Pending[Index].Stamp.Section != InSection ||
			!UsesSectionKey(Pending[Index].Kind))
		{
			continue;
		}

		QueuedInputBytes -=
			Pending[Index].InputBytes;

		FVoxelTaskRequest Request =
			MoveTemp(Pending[Index]);

		Pending.RemoveAtSwap(Index);

		RemoveActive(
			Request.Stamp,
			Request.Kind,
			Request.WorkClass);

		QueueCanceled(
			MoveTemp(Request));
	}

	for (FRunning& Task : Running)
	{
		if (UsesSectionKey(Task.Kind) &&
			Task.Stamp.Section == InSection)
		{
			Task.Slot->Cancel.Store(true);
		}
	}
}

void FVoxelTaskScheduler::CancelMatching(
	TFunctionRef<bool(EVoxelTaskKind, const FVoxelTaskStamp&)> InPredicate)
{
	check(IsInGameThread());
	for (int32 Index = Pending.Num() - 1; Index >= 0; --Index)
	{
		if (!InPredicate(Pending[Index].Kind, Pending[Index].Stamp))
		{
			continue;
		}
		QueuedInputBytes -= Pending[Index].InputBytes;
		FVoxelTaskRequest Request = MoveTemp(Pending[Index]);
		Pending.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		RemoveActive(Request.Stamp, Request.Kind, Request.WorkClass);
		QueueCanceled(MoveTemp(Request));
	}
	for (FRunning& Task : Running)
	{
		if (InPredicate(Task.Kind, Task.Stamp))
		{
			Task.Slot->Cancel.Store(true);
		}
	}
}

void FVoxelTaskScheduler::StopAndJoin()
{
	if (bStopped &&
		Running.IsEmpty())
	{
		return;
	}

	bStopped = true;

	for (FRunning& Task : Running)
	{
		Task.Slot->Cancel.Store(true);
	}

	for (FRunning& Task : Running)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Waiting for voxel task shutdown: kind=%d executeMs=%.2f"),
			static_cast<int32>(Task.Kind),
			Task.Slot->Result.ExecuteMilliseconds);
		Task.Task.Wait();
	}

	Pending.Reset();
	Running.Reset();
	Canceled.Reset();

	ActiveKeys.Reset();
	SectionTaskCounts.Reset();

	QueuedInputBytes = 0;
	ReservedBytes = 0;
	CriticalTaskCount = 0;
}

bool FVoxelTaskScheduler::Has(
	const FVoxelTaskStamp& InStamp,
	const EVoxelTaskKind InKind) const
{
	return ActiveKeys.Contains({
		InKind,
		InStamp
	});
}

bool FVoxelTaskScheduler::HasSectionTask(
	const FIntVector& InSection) const
{
	const int32* Count =
		SectionTaskCounts.Find(
			InSection);

	return Count &&
		*Count > 0;
}

int32 FVoxelTaskScheduler::ActiveCount() const
{
	return Running.Num();
}

int32 FVoxelTaskScheduler::CriticalCount() const
{
	return CriticalTaskCount;
}

void FVoxelTaskScheduler::SetBudget(
	const FVoxelTaskBudget& InBudget)
{
	check(IsInGameThread());

	Budget.MaxConcurrentTasks =
		FMath::Max(
			1,
			InBudget.MaxConcurrentTasks);

	Budget.MaxPendingTasks =
		FMath::Max(
			1,
			InBudget.MaxPendingTasks);

	Budget.MaxReservedBytes =
		FMath::Max<uint64>(
			1,
			InBudget.MaxReservedBytes);

	Budget.MaxInputBytes =
		FMath::Max<uint64>(
			1,
			InBudget.MaxInputBytes);

	Budget.MaxCompletedResultsPerFrame =
		FMath::Max(
			1,
			InBudget.MaxCompletedResultsPerFrame);

	Budget.MaxHeavyCompletedResultsPerFrame =
		FMath::Clamp(
			InBudget.MaxHeavyCompletedResultsPerFrame,
			1,
			Budget.MaxCompletedResultsPerFrame);

	Budget.MaxFineApplyPerFrame =
		FMath::Clamp(
			InBudget.MaxFineApplyPerFrame,
			1,
			Budget.MaxCompletedResultsPerFrame);
	Budget.MaxVoxelLODApplyPerFrame =
		FMath::Clamp(
			InBudget.MaxVoxelLODApplyPerFrame,
			1,
			Budget.MaxCompletedResultsPerFrame);
	Budget.MaxSurfaceApplyPerFrame =
		FMath::Clamp(
			InBudget.MaxSurfaceApplyPerFrame,
			1,
			Budget.MaxCompletedResultsPerFrame);
	Budget.MaxMacroApplyPerFrame =
		FMath::Clamp(
			InBudget.MaxMacroApplyPerFrame,
			1,
			Budget.MaxCompletedResultsPerFrame);

	Budget.MaxConcurrentSurfaceTasks =
		FMath::Max(1, InBudget.MaxConcurrentSurfaceTasks);
	Budget.MaxConcurrentMacroTasks =
		FMath::Max(1, InBudget.MaxConcurrentMacroTasks);

	Budget.MaxConcurrentCoarseTerrainTasks =
		FMath::Clamp(
			InBudget.MaxConcurrentCoarseTerrainTasks,
			1,
			Budget.MaxConcurrentTasks);

	Budget.MaxPendingCoarseTerrainTasks =
		FMath::Clamp(
			InBudget.MaxPendingCoarseTerrainTasks,
			1,
			Budget.MaxPendingTasks);
}

FVoxelTaskDiagnostics
FVoxelTaskScheduler::GetDiagnostics() const
{
	check(IsInGameThread());

	FVoxelTaskDiagnostics Result =
		Diagnostics;

	Result.Pending =
		Pending.Num();

	Result.Running =
		Running.Num();

	Result.Critical =
		CriticalTaskCount;

	Result.ReservedBytes =
		ReservedBytes;

	Result.QueuedInputBytes =
		QueuedInputBytes;

	Result.PendingByKind.Reset();
	for (const FVoxelTaskRequest& Request : Pending)
	{
		++Result.PendingByKind.FindOrAdd(Request.Kind);
	}
	Result.RunningByKind.Reset();
	for (const FRunning& Task : Running)
	{
		++Result.RunningByKind.FindOrAdd(Task.Kind);
	}

	return Result;
}

bool FVoxelTaskScheduler::IsHigherPriority(
	const FVoxelTaskRequest& InA,
	const FVoxelTaskRequest& InB)
{
	if ((InA.WorkClass == EVoxelWorkClass::Prefetch) != (InB.WorkClass == EVoxelWorkClass::Prefetch))
	{
		return InB.WorkClass == EVoxelWorkClass::Prefetch;
	}
	const bool bAVisual = IsVisualWorkClass(InA.WorkClass);
	const bool bBVisual = IsVisualWorkClass(InB.WorkClass);
	if (bAVisual != bBVisual)
	{
		return !bAVisual;
	}
	if (!bAVisual && InA.WorkClass != InB.WorkClass)
	{
		return
			static_cast<uint8>(
				InA.WorkClass) <
			static_cast<uint8>(
				InB.WorkClass);
	}
	if (InA.DistanceScore != InB.DistanceScore)
	{
		return
			InA.DistanceScore <
			InB.DistanceScore;
	}
	if (bAVisual && InA.WorkClass != InB.WorkClass)
	{
		return static_cast<uint8>(InA.WorkClass) < static_cast<uint8>(InB.WorkClass);
	}

	if (InA.ForwardScore !=
		InB.ForwardScore)
	{
		return
			InA.ForwardScore >
			InB.ForwardScore;
	}

	return
		InA.QueuedAt <
		InB.QueuedAt;
}

bool FVoxelTaskResult::HasHeavyApply() const
{
	if (bCanceled || !bSuccess) return false;
	if (Kind == EVoxelTaskKind::BuildFineMesh || Kind == EVoxelTaskKind::BuildVoxelProxy)
	{
		// 空网格只更新就绪/版本状态，不占据实体网格每帧一次的重发布配额。
		const TSharedPtr<FVoxelSectionMeshResult>& Mesh = Kind == EVoxelTaskKind::BuildFineMesh ? FineMesh : VoxelProxyMesh;
		return Mesh && Mesh->Batches.ContainsByPredicate([](const FVoxelRenderBatch& Batch)
		{
			return !Batch.Mesh.Triangles.IsEmpty();
		});
	}
	switch (Kind)
	{
	case EVoxelTaskKind::BuildCollision:
	case EVoxelTaskKind::BuildFineMesh:
	case EVoxelTaskKind::BuildVoxelProxy:
	case EVoxelTaskKind::BuildVolumeTransition:
	case EVoxelTaskKind::BuildSurface:
	case EVoxelTaskKind::BuildWater:
	case EVoxelTaskKind::BuildMacro:
	case EVoxelTaskKind::BuildDetails:
		return true;

	default:
		return false;
	}
}

bool FVoxelTaskScheduler::UsesSectionKey(
	const EVoxelTaskKind InKind)
{
	switch (InKind)
	{
	case EVoxelTaskKind::GenerateExactBase:
	case EVoxelTaskKind::BuildCollision:
	case EVoxelTaskKind::BuildFineMesh:
	case EVoxelTaskKind::DecodeOverlay:
	case EVoxelTaskKind::EncodeRegion:
		return true;

	default:
		return false;
	}
}

int32 FVoxelTaskScheduler::RunningCount(const EVoxelTaskKind InKind) const
{
	int32 Count = 0;
	for (const FRunning& Task : Running)
	{
		Count += Task.Kind == InKind ? 1 : 0;
	}
	return Count;
}

bool FVoxelTaskScheduler::CanStartKind(const EVoxelTaskKind InKind) const
{
	if (IsRepresentationDataKind(InKind))
	{
		const int32 DataRunning = RunningCount(EVoxelTaskKind::GenerateVoxelProxy) +
			RunningCount(EVoxelTaskKind::GenerateSurface) + RunningCount(EVoxelTaskKind::GenerateMacro);
		if (DataRunning >= FMath::Max(1, Budget.MaxConcurrentTasks / 2)) return false;
	}
	if (IsCoarseTerrainKind(InKind) &&
		RunningCount(EVoxelTaskKind::BuildSurface) +
			RunningCount(EVoxelTaskKind::BuildMacro) >=
			Budget.MaxConcurrentCoarseTerrainTasks)
	{
		return false;
	}

	switch (InKind)
	{
	case EVoxelTaskKind::BuildSurface:
		return RunningCount(InKind) < Budget.MaxConcurrentSurfaceTasks;
	case EVoxelTaskKind::BuildMacro:
		return RunningCount(InKind) < Budget.MaxConcurrentMacroTasks;
	default:
		return true;
	}
}

void FVoxelTaskScheduler::Pump()
{
	while (!bStopped &&
		Running.Num() <
			Budget.MaxConcurrentTasks &&
		!Pending.IsEmpty())
	{
		int32 BestIndex =
			INDEX_NONE;

		for (int32 Index = 0;
			Index < Pending.Num();
			++Index)
		{
			const FVoxelTaskRequest& Request =
				Pending[Index];

			if (!CanStartKind(Request.Kind))
			{
				continue;
			}

			if (Request.ReservedBytes >
				Budget.MaxReservedBytes -
					ReservedBytes)
			{
				continue;
			}

			if (BestIndex == INDEX_NONE ||
				IsHigherPriority(
					Request,
					Pending[BestIndex]))
			{
				BestIndex =
					Index;
			}
		}

		if (BestIndex ==
			INDEX_NONE)
		{
			break;
		}

		// 超过等待期限的可视任务按入队时间获得机会；碰撞、出生及交互优先级不降级。
		if (IsVisualWorkClass(Pending[BestIndex].WorkClass))
		{
			const double Deadline = FPlatformTime::Seconds() - 1.0;
			int32 Oldest = INDEX_NONE;
			for (int32 Index = 0; Index < Pending.Num(); ++Index)
			{
				const FVoxelTaskRequest& Candidate = Pending[Index];
				if (IsVisualWorkClass(Candidate.WorkClass) && Candidate.QueuedAt <= Deadline &&
					CanStartKind(Candidate.Kind) && Candidate.ReservedBytes <= Budget.MaxReservedBytes - ReservedBytes &&
					(Oldest == INDEX_NONE || Candidate.QueuedAt < Pending[Oldest].QueuedAt))
				{
					Oldest = Index;
				}
			}
			if (Oldest != INDEX_NONE) BestIndex = Oldest;
		}

		FVoxelTaskRequest Request =
			MoveTemp(
				Pending[BestIndex]);

		Pending.RemoveAtSwap(
			BestIndex);

		QueuedInputBytes -=
			Request.InputBytes;

		ReservedBytes +=
			Request.ReservedBytes;

		FRunning RunningTask;
		RunningTask.Stamp =
			Request.Stamp;
		RunningTask.Kind =
			Request.Kind;
		RunningTask.WorkClass =
			Request.WorkClass;
		RunningTask.QueuedAt =
			Request.QueuedAt;
		RunningTask.ReservedBytes =
			Request.ReservedBytes;
		RunningTask.Slot =
			MakeShared<
				FSlot,
				ESPMode::ThreadSafe>();
		RunningTask.Apply =
			MoveTemp(Request.Apply);

		const TSharedPtr<
			FSlot,
			ESPMode::ThreadSafe> Slot =
				RunningTask.Slot;

		const FVoxelTaskStamp Stamp =
			RunningTask.Stamp;

		const EVoxelTaskKind Kind =
			RunningTask.Kind;

		const uint64 ResultBudget =
			RunningTask.ReservedBytes;

		const double QueuedAt =
			RunningTask.QueuedAt;

		RunningTask.Task =
			UE::Tasks::Launch(
				UE_SOURCE_LOCATION,
				[
					Slot,
					Stamp,
					Kind,
					ResultBudget,
					QueuedAt,
					Execute =
						MoveTemp(
							Request.Execute)
				]() mutable
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(
						Voxel_TaskExecute);

					const double Start =
						FPlatformTime::Seconds();

					if (!Slot->Cancel.Load())
					{
						Slot->Result =
							Execute(
								Slot->Cancel);
					}

					const double End =
						FPlatformTime::Seconds();

					Slot->Result.Stamp =
						Stamp;

					Slot->Result.Kind =
						Kind;

					Slot->Result.bCanceled =
						Slot->Cancel.Load();

					Slot->Result.QueueMilliseconds =
						FMath::Max(
							0.0,
							(Start - QueuedAt) *
								1000.0);

					Slot->Result.ExecuteMilliseconds =
						FMath::Max(
							0.0,
							(End - Start) *
								1000.0);

					if (Slot->Result.ResultBytes() >
						ResultBudget)
					{
						FVoxelTaskResult ErrorResult;

						ErrorResult.Stamp =
							Stamp;

						ErrorResult.Kind =
							Kind;

						ErrorResult.Error =
							TEXT(
								"Voxel task result memory budget exceeded");

						ErrorResult.QueueMilliseconds =
							Slot->Result.QueueMilliseconds;

						ErrorResult.ExecuteMilliseconds =
							Slot->Result.ExecuteMilliseconds;

						Slot->Result =
							MoveTemp(
								ErrorResult);
					}
				});

		Running.Add(
			MoveTemp(RunningTask));
	}
}

void FVoxelTaskScheduler::QueueCanceled(
	FVoxelTaskRequest&& InRequest)
{
	FCompleted Completed;

	Completed.Result.Stamp =
		InRequest.Stamp;

	Completed.Result.Kind =
		InRequest.Kind;

	Completed.Result.bCanceled =
		true;

	Completed.Apply =
		MoveTemp(InRequest.Apply);

	Canceled.Add(
		MoveTemp(Completed));
}

void FVoxelTaskScheduler::AddActive(
	const FVoxelTaskStamp& InStamp,
	const EVoxelTaskKind InKind,
	const EVoxelWorkClass InWorkClass)
{
	ActiveKeys.Add({
		InKind,
		InStamp
	});

	if (UsesSectionKey(InKind))
	{
		++SectionTaskCounts.
			FindOrAdd(
				InStamp.Section);
	}

	if (InWorkClass ==
		EVoxelWorkClass::Critical)
	{
		++CriticalTaskCount;
	}
}

void FVoxelTaskScheduler::RemoveActive(
	const FVoxelTaskStamp& InStamp,
	const EVoxelTaskKind InKind,
	const EVoxelWorkClass InWorkClass)
{
	ActiveKeys.Remove({
		InKind,
		InStamp
	});

	if (UsesSectionKey(InKind))
	{
		if (int32* Count =
			SectionTaskCounts.Find(
				InStamp.Section))
		{
			--(*Count);

			if (*Count <= 0)
			{
				SectionTaskCounts.Remove(
					InStamp.Section);
			}
		}
	}

	if (InWorkClass ==
		EVoxelWorkClass::Critical)
	{
		CriticalTaskCount =
			FMath::Max(
				0,
				CriticalTaskCount - 1);
	}
}

void FVoxelTaskScheduler::RecordCompletedResult(
	const FVoxelTaskResult& InResult)
{
	FVoxelTaskKindDiagnostics& Kind =
		Diagnostics.ByKind.
			FindOrAdd(
				InResult.Kind);

	++Kind.Completed;

	if (InResult.bCanceled)
	{
		++Kind.Canceled;
	}
	else if (!InResult.bSuccess)
	{
		++Kind.Failed;
	}

	Kind.TotalQueueMilliseconds +=
		InResult.QueueMilliseconds;

	Kind.TotalExecuteMilliseconds +=
		InResult.ExecuteMilliseconds;

	Kind.TotalApplyMilliseconds +=
		InResult.ApplyMilliseconds;

	Kind.MaximumQueueMilliseconds =
		FMath::Max(
			Kind.MaximumQueueMilliseconds,
			InResult.QueueMilliseconds);

	Kind.MaximumExecuteMilliseconds =
		FMath::Max(
			Kind.MaximumExecuteMilliseconds,
			InResult.ExecuteMilliseconds);

	Kind.MaximumApplyMilliseconds =
		FMath::Max(
			Kind.MaximumApplyMilliseconds,
			InResult.ApplyMilliseconds);
}
