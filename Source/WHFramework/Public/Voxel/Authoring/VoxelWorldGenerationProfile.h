#pragma once

#include "CoreMinimal.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"
#include "VoxelWorldGenerationProfile.generated.h"

class UVoxelData;
class UVoxelBiomeData;
class UVoxelFeatureData;
class UVoxelStructureData;
class UVoxelSurfaceRuleSet;
class UVoxelDetailData;
class UVoxelViewProfile;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelWeightedPlantReference
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UVoxelData> Plant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 Weight = 100;
};

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelWorldGenerationProfile : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UVoxelWorldGenerationProfile();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation", meta = (ClampMin = "1"))
	int32 TargetCellCentimeters = 25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FVoxelGenerationSettings Defaults;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Blocks")
	TSoftObjectPtr<UVoxelData> Stone;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Blocks")
	TSoftObjectPtr<UVoxelData> Dirt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Blocks")
	TSoftObjectPtr<UVoxelData> Grass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Blocks")
	TSoftObjectPtr<UVoxelData> Sand;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Blocks")
	TSoftObjectPtr<UVoxelData> Snow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Blocks")
	TSoftObjectPtr<UVoxelData> Water;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Blocks")
	TSoftObjectPtr<UVoxelData> Lava;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Blocks")
	TSoftObjectPtr<UVoxelData> Bedrock;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Blocks")
	TSoftObjectPtr<UVoxelData> Road;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	TArray<TSoftObjectPtr<UVoxelBiomeData>> Biomes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	TArray<TSoftObjectPtr<UVoxelFeatureData>> Features;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	TArray<TSoftObjectPtr<UVoxelStructureData>> Structures;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	TArray<TSoftObjectPtr<UVoxelSurfaceRuleSet>> SurfaceRuleSets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	TArray<TSoftObjectPtr<UVoxelDetailData>> Details;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation|Ecology")
	TArray<FVoxelWeightedPlantReference> FlowerPalette;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|View")
	TSoftObjectPtr<UVoxelViewProfile> View;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Bake")
	TArray<uint8> RecipeBytes;

	UPROPERTY(VisibleAnywhere, Category = "Voxel|Bake")
	uint64 RecipeHash = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Bake")
	int32 RecipeBakeVersion = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Bake")
	int32 BakedCellCentimeters = 0;
};
