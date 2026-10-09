#include "Voxel/Authoring/VoxelFineStreamingPolicy.h"
#include "Voxel/Task/VoxelTerrainResources.h"

FVoxelFineWindowSettings FVoxelFineStreamingPolicy::WindowSettings() const
{
	FVoxelFineWindowSettings Result;
	Result.RadiusCells = ResidencyHorizontalHalfExtentCells;
	Result.VerticalHalfExtentCells = ResidencyVerticalHalfExtentCells;
	Result.PreferredShardSideCells = ShardSideCells;
	Result.ShiftStepCells = HorizontalShiftStepCells;
	Result.VerticalShiftStepCells = VerticalShiftStepCells;
	Result.GuardMarginCells = GuardMarginCells;
	Result.MaximumShardCount = MaximumWindowShards;
	Result.MinimumPreparationSeconds = MinimumPreparationSeconds;
	Result.MaximumPreparationSeconds = MaximumPreparationSeconds;
	return Result;
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

bool FVoxelFineStreamingPolicy::Validate(FString& OutError) const
{
	if (!WindowSettings().Validate(InteractiveRadiusCells, CollisionGuardRadiusCells, OutError))
	{
		return false;
	}
	if (PolicyVersion != 1 || InteractiveVerticalHalfExtentCells <= 0 ||
		ResidencyVerticalHalfExtentCells <= InteractiveVerticalHalfExtentCells + 8 + GuardMarginCells + VerticalShiftStepCells ||
		MaximumPageLevel < 1 || MaximumPageLevel > 4 || ShardSideCells > (16 << MaximumPageLevel) ||
		FirstMergeDistanceCells < ShardSideCells || FirstMergeDistanceCells > (16 << MaximumPageLevel) ||
		!FMath::IsFinite(PredictionSeconds) || PredictionSeconds < 0.0 ||
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
