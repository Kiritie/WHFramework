#pragma once

#include "CoreMinimal.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"
#include "VoxelBiomeData.generated.h"

class UVoxelSurfaceRuleSet;
class UVoxelData;
class UVoxelFeatureData;
class UVoxelStructureData;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelBiomeData : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UVoxelBiomeData();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FName StableId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FVoxelGenerationRange Temperature;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FVoxelGenerationRange Moisture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FVoxelGenerationRange Continentalness;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FVoxelGenerationRange Erosion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FVoxelGenerationRange Height;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FVoxelGenerationRange Slope;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	TSoftObjectPtr<UVoxelSurfaceRuleSet> SurfaceRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	TSoftObjectPtr<UVoxelData> DefaultSurface;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FVoxelBiomeEcologyModifier Ecology;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	TArray<TSoftObjectPtr<UVoxelFeatureData>> Features;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	TArray<TSoftObjectPtr<UVoxelStructureData>> Structures;
};
