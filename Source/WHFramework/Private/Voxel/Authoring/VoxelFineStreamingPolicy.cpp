#include "Voxel/Authoring/VoxelFineStreamingPolicy.h"

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
		MaximumResidentGeometryMegabytes < MaximumShardGeometryMegabytes ||
		ReservedNearWorkers < 1 || !FMath::IsFinite(MaximumBackgroundWaitSeconds) || MaximumBackgroundWaitSeconds <= 0.0)
	{
		OutError = TEXT("Fine 交互、Page、内存或调度配置不合法");
		return false;
	}
	OutError.Reset();
	return true;
}
