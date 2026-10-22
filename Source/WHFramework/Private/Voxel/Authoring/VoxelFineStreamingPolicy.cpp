#include "Voxel/Authoring/VoxelFineStreamingPolicy.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

FVoxelFineWindowSettings FVoxelFineStreamingPolicy::WindowSettings(const FVoxelStreamingSourceView& InView) const
{
	FVoxelFineWindowSettings Result;
	Result.RadiusCells = InView.FineRadiusCells;
	Result.VerticalHalfExtentCells = InView.FineVerticalHalfExtentCells;
	Result.PreferredShardSideCells = ShardSideCells;
	const int32 Side = FMath::Max(1, ShardSideCells);
	const int64 AvailableShift = int64(InView.FineRadiusCells) - InView.CollisionGuardRadiusCells - 14 - Side - 1;
	const int64 PreferredShift = FMath::Min<int64>(InView.FineRadiusCells / 2, AvailableShift);
	Result.ShiftStepCells = int32(FMath::Max<int64>(Side, PreferredShift / Side * Side));
	Result.VerticalShiftStepCells = Side;
	const int64 HorizontalMargin = int64(InView.FineRadiusCells) - InView.CollisionGuardRadiusCells - 14 - Result.ShiftStepCells - 1;
	const int64 VerticalMargin = int64(InView.FineVerticalHalfExtentCells) -
		FMath::Max<int64>(InView.CollisionGuardRadiusCells + 14ll, InView.FineLevel0VerticalHalfExtentCells + 8ll) - Side - 1;
	Result.GuardMarginCells = int32(FMath::Clamp<int64>(FMath::Min(HorizontalMargin, VerticalMargin), 0, Side));
	Result.MaximumShardCount = MaximumWindowShards;
	Result.MinimumPreparationSeconds = MinimumPreparationSeconds;
	Result.MaximumPreparationSeconds = MaximumPreparationSeconds;
	return Result;
}

int32 FVoxelFineStreamingPolicy::MinimumRadiusCells(const FVoxelStreamingSourceView& InView) const
{
	const int64 Side = FMath::Max(1, ShardSideCells);
	const int64 Required = int64(InView.CollisionGuardRadiusCells) + 14 + Side + 1;
	return int32(FMath::Min<int64>(MAX_int32, (Required + Side - 1) / Side * Side));
}

FVoxelTerrainResourceLimits FVoxelFineStreamingPolicy::GeometryLimits() const
{
	FVoxelTerrainResourceLimits Result;
	Result.InputBytes = uint64(MaximumInputGeometryMegabytes) * 1024 * 1024;
	Result.WorkingBytes = uint64(MaximumWorkingGeometryMegabytes) * 1024 * 1024;
	Result.StagedBytes = uint64(MaximumStagedGeometryMegabytes) * 1024 * 1024;
	Result.ResidentBytes = uint64(MaximumResidentGeometryMegabytes) * 1024 * 1024;
	Result.OverlapBytes = uint64(ResidentGeometryOverlapMegabytes) * 1024 * 1024;
	return Result;
}

bool FVoxelFineStreamingPolicy::Validate(const FVoxelStreamingSourceView& InView, FString& OutError) const
{
	const auto Window = WindowSettings(InView);
	if (!Window.Validate(InView.FineLevel0RadiusCells, InView.CollisionGuardRadiusCells, OutError))
	{
		return false;
	}
	if (PolicyVersion != 1 || InView.FineLevel0VerticalHalfExtentCells <= 0 ||
		int64(InView.FineVerticalHalfExtentCells) <= int64(InView.FineLevel0VerticalHalfExtentCells) + 8 + Window.GuardMarginCells + Window.VerticalShiftStepCells ||
		MaximumPageLevel < 1 || MaximumPageLevel > 4 || ShardSideCells > (16 << MaximumPageLevel) ||
		FirstMergeDistanceCells < ShardSideCells || FirstMergeDistanceCells > (16 << MaximumPageLevel) ||
		!FMath::IsFinite(InView.FinePredictionSeconds) || InView.FinePredictionSeconds < 0.0 ||
		MaximumMeshPieceMegabytes < 1 || MaximumMeshPieceMegabytes > 64 ||
		MaximumShardGeometryMegabytes < 1 || MaximumShardGeometryMegabytes > 1024 ||
		MaximumInputGeometryMegabytes < 1 || MaximumWorkingGeometryMegabytes < 1 || MaximumStagedGeometryMegabytes < 1 ||
		MaximumResidentGeometryMegabytes < MaximumShardGeometryMegabytes || ResidentGeometryOverlapMegabytes < 1 ||
		!GeometryLimits().IsValid() || ReservedNearWorkers < 1 || !FMath::IsFinite(MaximumBackgroundWaitSeconds) || MaximumBackgroundWaitSeconds <= 0.0)
	{
		OutError = TEXT("Fine 交互、Page、内存或调度配置不合法");
		return false;
	}
	OutError.Reset();
	return true;
}
