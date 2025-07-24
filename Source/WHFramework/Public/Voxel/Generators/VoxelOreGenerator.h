// Fill out your copyright notice in the Description page of Project Settings.

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

	FVoxelOreGenerateData(const EVoxelType VoxelType, int32 HeightRange, const float SpawnRate, const int MinSize, const int MaxSize)
		: VoxelType(VoxelType),
		  MaxHeight(HeightRange),
		  SpawnRate(SpawnRate),
		  MinSize(MinSize),
		  MaxSize(MaxSize)
	{
	}
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EVoxelType VoxelType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int MinSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int MaxSize;
};

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelOreGenerator : public UVoxelGenerator
{
	GENERATED_BODY()
	
public:
	UVoxelOreGenerator();
	
public:
	virtual void Generate(AVoxelChunk* InChunk) override;

protected:
	void GenerateOre(FIndex InIndex, const FVoxelOreGenerateData& InGenerateData);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVoxelOreGenerateData> GenerateDatas;
};
