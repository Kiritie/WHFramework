#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Math/MathTypes.h"
#include "VoxelRiverGenerator.generated.h"

struct FVoxelTopography;

/** 体素河流生成器 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelRiverGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelRiverGenerator();

	virtual void Generate(UVoxelChunk* InChunk) override;

	bool ApplyToTopography(FIndex InWorldIndex, FVoxelTopography& InOutTopography) const;

	float SampleNormalizedRiverDistance(FIndex InWorldIndex) const;

	FVector2D SampleRiverDirection(FIndex InWorldIndex) const;

	int32 SampleRiverWaterHeight(FIndex InWorldIndex) const;

protected:
	float GetRiverDistance(FIndex InWorldIndex, const TMap<FIndex, int32>* InHeightCache = nullptr) const;

	float GetBaseRiverField(FIndex InWorldIndex) const;

	float GetTerrainAwareRiverField(FIndex InWorldIndex, const TMap<FIndex, int32>* InHeightCache = nullptr) const;

	float CalculateTerrainDifficulty(FIndex InWorldIndex, const TMap<FIndex, int32>* InHeightCache = nullptr) const;

	int32 CalculateWaterHeight() const;

	bool ApplyToTopographyCached(FIndex InWorldIndex, FVoxelTopography& InOutTopography, const TMap<FIndex, int32>& InHeightCache) const;

	bool ApplyRiverProfile(FIndex InWorldIndex, float InRiverDistance, FVoxelTopography& InOutTopography, int32 InWaterHeight) const;

	bool ApplyRiverShore(FIndex InWorldIndex, float InRiverDistance, FVoxelTopography& InOutTopography, int32 InWaterHeight) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0001"))
	float RiverScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0001"))
	float WarpScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.001"))
	float RiverWidth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.001"))
	float BankWidth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 RiverDepth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 RiverHeightAboveSea;
};
