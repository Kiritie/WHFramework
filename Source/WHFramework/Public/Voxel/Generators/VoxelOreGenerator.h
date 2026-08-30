
#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelOreGenerator.generated.h"

USTRUCT(BlueprintType)
struct FVoxelOreGenerateData
{
	GENERATED_BODY()

public:
	FVoxelOreGenerateData()
	{
		VoxelType = EVoxelType::Empty;
		MaxHeight = -1;
		SpawnRate = 0.f;
		MinSize = 1;
		MaxSize = 1;
	}

	FVoxelOreGenerateData(EVoxelType InVoxelType, int32 InMaxHeight, float InSpawnRate, int32 InMinSize, int32 InMaxSize)
		: VoxelType(InVoxelType),
		  MaxHeight(InMaxHeight),
		  SpawnRate(InSpawnRate),
		  MinSize(InMinSize),
		  MaxSize(InMaxSize)
	{
	}
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EVoxelType VoxelType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxSize;
};

/** 体素矿物生成器 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelOreGenerator : public UVoxelGenerator
{
	GENERATED_BODY()
	
public:
	UVoxelOreGenerator();
	
public:
	virtual void Generate(UVoxelChunk* InChunk) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVoxelOreGenerateData> GenerateDatas;
};
