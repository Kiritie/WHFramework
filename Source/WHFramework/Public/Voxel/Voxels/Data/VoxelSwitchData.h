#pragma once

#include "VoxelData.h"
#include "VoxelInteractData.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelSwitchData.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelSwitchData : public UVoxelInteractData
{
	GENERATED_BODY()

public:
	UVoxelSwitchData();

public:
	virtual const FVoxelMeshData& GetMeshData(const FVoxelItem& InVoxelItem) const override;
};
