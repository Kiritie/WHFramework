#pragma once

#include "CoreMinimal.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "Parameter/ParameterTypes.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "VoxelFeatureData.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelFeatureData : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UVoxelFeatureData();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FName StableId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FName AlgorithmId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	EVoxelGenerationStage Stage = EVoxelGenerationStage::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FVoxelFeaturePlacement Placement;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FParameter Configuration;
};
