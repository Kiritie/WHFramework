// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "VoxelHeightGenerator.generated.h"

USTRUCT(BlueprintType)
struct FVoxelHeightGenerateData
{
	GENERATED_BODY()

public:
	FVoxelHeightGenerateData()
	{
		CrystalSize = 0.f;
		MaxHeight = 0;
		Weight = 1.0f;
	}

	FVoxelHeightGenerateData(const float CrystalSize, const int32 MaxHeight, const float Weight)
		: CrystalSize(CrystalSize),
		  MaxHeight(MaxHeight),
		  Weight(Weight)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CrystalSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Weight;
};

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelHeightGenerator : public UVoxelGenerator
{
	GENERATED_BODY()
	
public:
	UVoxelHeightGenerator();
	
public:
	virtual void Generate(AVoxelChunk* InChunk) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVoxelHeightGenerateData> GenerateDatas;
};
