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
		CrystalSize = 0;
		Weight = 0.f;
		MaxHeight = 0;
	}

	FVoxelHeightGenerateData(const int32 CrystalSize, const FVector& NoiseScale, const int32 MaxHeight, const float Weight)
		: CrystalSize(CrystalSize),
		  NoiseScale(NoiseScale),
		  MaxHeight(MaxHeight),
		  Weight(Weight)
	{
	}
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CrystalSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector NoiseScale;

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
