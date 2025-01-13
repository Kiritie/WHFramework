#pragma once

#include "VoxelSwitchData.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelContainerData.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelContainerData : public UVoxelSwitchData
{
	GENERATED_BODY()

public:
	UVoxelContainerData();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FInventorySaveData InventoryData;
};
