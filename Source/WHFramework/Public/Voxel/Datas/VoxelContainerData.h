#pragma once

#include "VoxelInteractData.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelContainerData.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelContainerData : public UVoxelInteractData
{
	GENERATED_BODY()

public:
	UVoxelContainerData();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FInventorySaveData InventoryData;
};
