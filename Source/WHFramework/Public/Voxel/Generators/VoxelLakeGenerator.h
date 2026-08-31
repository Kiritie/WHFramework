
#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Math/MathTypes.h"
#include "VoxelLakeGenerator.generated.h"

struct FVoxelTopography;

struct FVoxelLakeFeature
{
	FIndex Center;
	float MajorRadius = 0.f;
	float MinorRadius = 0.f;
	float Rotation = 0.f;
	float Depth = 0.f;
};

/** 体素湖泊生成器 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelLakeGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelLakeGenerator();

public:
	virtual void Generate(UVoxelChunk* InChunk) override;

	bool ApplyToTopography(FIndex InWorldIndex, FVoxelTopography& InOutTopography) const;

	void GenerateSprings(UVoxelChunk* InChunk) const;

protected:
	bool TryCreateLakeFeature(FIndex InChunkIndex, FVoxelLakeFeature& OutFeature) const;

	bool BuildLakeFeature(FIndex InChunkIndex, FVoxelLakeFeature& OutFeature) const;

	void GetLakeFeatures(FIndex InMinWorldIndex, FIndex InMaxWorldIndex, TArray<FVoxelLakeFeature>& OutFeatures) const;

	bool EvaluateLake(const FVoxelLakeFeature& InFeature, FIndex InWorldIndex, int32& OutBedHeight, int32& OutWaterHeight) const;

	float CalculateShapeAlpha(const FVoxelLakeFeature& InFeature, FIndex InWorldIndex) const;

	bool TryGetSpringSource(FIndex InChunkIndex, FIndex& OutSource) const;

	void GenerateSpringSlice(UVoxelChunk* InChunk, FIndex InSource) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnRate;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NoiseScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinDistance;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxRadius;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinDepth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxDepth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SpringSeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpringSpawnRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpringMaxDistance;

private:
	mutable FRWLock LakeFeatureCacheLock;

	mutable TSet<FIndex> EvaluatedLakeAnchors;

	mutable TMap<FIndex, FVoxelLakeFeature> LakeFeatureCache;
};
