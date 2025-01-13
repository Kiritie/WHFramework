#pragma once

#include "VoxelData.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelInteractData.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelInteractData : public UVoxelData
{
	GENERATED_BODY()

public:
	UVoxelInteractData();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<EVoxelInteractAction> InteractActions;
};
