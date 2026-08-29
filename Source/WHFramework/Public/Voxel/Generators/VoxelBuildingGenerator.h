// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Math/MathTypes.h"
#include "VoxelGenerator.h"
#include "VoxelBuildingGenerator.generated.h"

class UVoxelPrefabData;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelBuildingGenerateData
{
	GENERATED_BODY()

public:
	FVoxelBuildingGenerateData()
	{
		PrefabAsset = FPrimaryAssetId();
		Chance = 1.f;
		FrontDirection = ERightAngle::RA_270;
	}

	FVoxelBuildingGenerateData(const FPrimaryAssetId& InPrefabAsset, const float InChance, const ERightAngle InFrontDirection = ERightAngle::RA_270)
		: PrefabAsset(InPrefabAsset), Chance(InChance), FrontDirection(InFrontDirection)
	{
	}

public:
	/** Building prefab to place. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowedTypes = "VoxelPrefab"))
	FPrimaryAssetId PrefabAsset;

	/** Relative selection chance. Effective probability is Chance divided by the sum of all valid chances. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float Chance;

	/** Local direction of the building entrance: 0=+X, 90=+Y, 180=-X, 270=-Y. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ERightAngle FrontDirection;
};

/**
 * Places at most one weighted-random building at the center of a generated chunk.
 * It intentionally performs no settlement layout or path generation.
 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelBuildingGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelBuildingGenerator();

public:
	virtual void Initialize(UVoxelModule* InModule) override;

	virtual void Generate(UVoxelChunk* InChunk) override;

protected:
	int32 SelectBuildingIndex(const FVector2D& InRandomPosition) const;

	bool PlaceBuilding(int32 InX, int32 InY, int32 InBuildingIndex);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	int32 Seed;

	/** Probability that a generated chunk attempts to place one building. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	TArray<FVoxelBuildingGenerateData> GenerateDatas;

private:
	UPROPERTY(Transient)
	TArray<UVoxelPrefabData*> _PrefabAssets;
};
