#pragma once

#include "VoxelData.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelTreeData.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelTreeData : public UVoxelData
{
	GENERATED_BODY()

public:
	UVoxelTreeData();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D TreeHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UVoxelData* LeavesData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D LeavesHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D LeavesWidth;
};
