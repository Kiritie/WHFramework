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
	virtual const FVoxelMeshData& GetMeshData(const FVoxelItem& InVoxelItem) const override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<EInteractAction> InteractActions;
};
