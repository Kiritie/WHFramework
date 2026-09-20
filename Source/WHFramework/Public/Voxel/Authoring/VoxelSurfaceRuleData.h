#pragma once

#include "CoreMinimal.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "VoxelSurfaceRuleData.generated.h"

class UVoxelData;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelSurfaceRuleData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVoxelGenerationRange Height;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVoxelGenerationRange Slope;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVoxelGenerationRange Temperature;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVoxelGenerationRange Moisture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 MinDepth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 MaxDepth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UVoxelData> Block;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRiverOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bLakeOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOceanOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCoastOnly = false;
};

UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelSurfaceRuleSet : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UVoxelSurfaceRuleSet();

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	FName StableId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Generation")
	TArray<FVoxelSurfaceRuleData> Rules;
};
