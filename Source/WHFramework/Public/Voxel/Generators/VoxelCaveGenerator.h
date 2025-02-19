// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Math/MathTypes.h"
#include "VoxelCaveGenerator.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelCaveGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelCaveGenerator();

public:
	virtual void Generate(AVoxelChunk* InChunk) override;

protected:
	bool IsCave(FIndex InIndex) const;

protected:
	void GenerateCaveEntry(FIndex InIndex) const;

	FIndex FindNearestCaveEntryIndex(const FIndex& InStartIndex, const FVector& InDirection, int32 InMaxSearchDistance) const;

	FIndex FindNearestCaveEntryIndexMultiDirection(const FIndex& InStartIndex, int32 InMaxSearchDistance) const;

	TArray<FIndex> GenerateCaveEntrySlopePath(FIndex InStartIndex, FIndex InEndIndex) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Times;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CrystalSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxDepth;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnEntryRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinEntryRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxEntryRadius;
};
