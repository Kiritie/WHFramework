#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"

enum class EVoxelWaterKind : uint8
{
	None = 0,
	River,
	Lake,
	Ocean
};

struct WHFRAMEWORK_API FVoxelWaterSurfaceTileData
{
	FVoxelSurfaceTileKey Key;
	uint64 Revision = 0;
	int32 Side = 0;
	int32 Step = 0;
	TArray<int32> WaterZ;
	TArray<uint8> WaterKind;
};

class WHFRAMEWORK_API FVoxelWaterViewBuilder
{
public:
	bool Build(
		const FVoxelSurfaceTileData& InSurface,
		FVoxelWaterSurfaceTileData& OutWater,
		FString& OutError) const;
};
