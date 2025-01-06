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
		CrystalSize = 2.f;
		MaxHeight = -1;
	}

	FVoxelOreGenerateData(const EVoxelType VoxelType, const float CrystalSize, const int32 MaxHeight)
		: VoxelType(VoxelType),
		  CrystalSize(CrystalSize),
		  MaxHeight(MaxHeight)
	{
	}
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EVoxelType VoxelType;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CrystalSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxHeight;
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVoxelOreGenerateData> GenerateDatas;
};
