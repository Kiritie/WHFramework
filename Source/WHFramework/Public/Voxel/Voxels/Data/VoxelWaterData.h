#pragma once

#include "VoxelData.h"

#include "VoxelWaterData.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelWaterData : public UVoxelData
{
	GENERATED_BODY()

public:
	UVoxelWaterData();

public:
	float GetWaterHeight(const FVoxelItem& InVoxelItem) const;

	float GetWaterCornerHeight(const FVoxelItem& InVoxelItem, int32 InX, int32 InY) const;

	bool ShouldBuildWaterFace(const FVoxelItem& InVoxelItem, EDirection InFacing) const;
};
