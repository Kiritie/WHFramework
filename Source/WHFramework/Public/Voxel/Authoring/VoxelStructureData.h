#pragma once

#include "CoreMinimal.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "VoxelStructureData.generated.h"

class UVoxelData;
class UVoxelPrefabData;
class UVoxelDetailData;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelGridBox
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntVector Min = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntVector Max = FIntVector(1, 1, 1);
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelStaticBlockMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UVoxelData> Source;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UVoxelData> SolidReplacement;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelStructureDetailSocket
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UVoxelDetailData> Detail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntVector SourceCorner = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "3"))
	uint8 Yaw = 0;
};

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelStructureData : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UVoxelStructureData();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FName StableId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	EVoxelGenerationStage Stage = EVoxelGenerationStage::SurfaceStructures;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FVoxelStructurePlacement Placement;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	EVoxelStructureAdaptation Adaptation = EVoxelStructureAdaptation::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FName LayoutAlgorithmId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Source")
	TSoftObjectPtr<UVoxelPrefabData> SourcePrefab;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Source", meta = (ClampMin = "1"))
	int32 SourceCellCentimeters = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Source")
	int32 SourceGroundCellZ = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Source")
	FIntVector SourceEntranceCorner = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Source")
	FVoxelGridBox SourceBounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Source")
	TArray<FVoxelGridBox> ClearVolumes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Source")
	TArray<FVoxelStaticBlockMapping> StaticMappings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Source")
	TArray<FVoxelStructureConnectorData> Connectors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Source")
	TArray<FVoxelStructureDetailSocket> Details;
};
