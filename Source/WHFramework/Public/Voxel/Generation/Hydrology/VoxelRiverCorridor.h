#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrology.h"

struct WHFRAMEWORK_API FVoxelRiverProjection
{
    int32 WaterPlane = 0;
    int32 DistanceCells = 0;
    FVoxelRiverShape Shape;
};

namespace VoxelHydrology
{
    WHFRAMEWORK_API bool ProjectRiverSegment(const FVoxelRiverMeanderPoint& InA,
        const FVoxelRiverMeanderPoint& InB, const FIntPoint& InSample,
        FVoxelRiverProjection& OutProjection);

    WHFRAMEWORK_API bool SampleRiverCorridorCap(const FVoxelRiverMeanderPoint& InA,
        const FVoxelRiverMeanderPoint& InB, const FVoxelTerrainGenerator& InTerrain,
        TFunctionRef<bool(const FIntPoint&)> InIsReceivingWater,
        int32& OutUpperWaterPlane, FIntPoint& OutLowestCell,
        FString& OutError, const TAtomic<bool>* InCancel = nullptr);

    WHFRAMEWORK_API bool SampleRiverCorridorCap(const FVoxelRiverMeanderPoint& InA,
        const FVoxelRiverMeanderPoint& InB, const FVoxelTerrainGenerator& InTerrain,
        int32& OutUpperWaterPlane, FString& OutError, const TAtomic<bool>* InCancel = nullptr);
}
