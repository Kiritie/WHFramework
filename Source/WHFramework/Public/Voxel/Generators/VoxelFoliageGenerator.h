
#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelFoliageGenerator.generated.h"

struct FVoxelTreeFeature
{
	FIndex Root;
	int32 Height = 4;
	int32 LeafRadius = 2;
	EVoxelType WoodType = EVoxelType::Oak;
	EVoxelType LeafType = EVoxelType::Oak_Leaves;
};

/** 体素植被生成器 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelFoliageGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelFoliageGenerator();

public:
	virtual void Generate(UVoxelChunk* InChunk) override;

protected:
	void GeneratePlants(UVoxelChunk* InChunk) const;

	void GenerateTrees(UVoxelChunk* InChunk) const;

	bool TryCreateTreeFeature(FIndex InWorldIndex, FVoxelTreeFeature& OutFeature) const;

	void PlaceTreeSlice(UVoxelChunk* InChunk, const FVoxelTreeFeature& InFeature) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GrassRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FlowerRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TreeRate;
};
