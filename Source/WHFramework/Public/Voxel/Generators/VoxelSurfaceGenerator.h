#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Math/MathTypes.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelSurfaceGenerator.generated.h"

USTRUCT(BlueprintType)
struct FVoxelSurfaceNoiseLayer
{
	GENERATED_BODY()

	FVoxelSurfaceNoiseLayer() = default;

	FVoxelSurfaceNoiseLayer(float InScale, float InWeight)
		: Scale(InScale), Weight(InWeight)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Scale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Weight = 1.f;
};

/** 体素地表生成器 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelSurfaceGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelSurfaceGenerator();

	virtual void Generate(UVoxelChunk* InChunk) override;

	FVoxelTopography SampleTopography(FIndex InWorldIndex) const;

protected:
	float SampleTemperature(FIndex InWorldIndex, int32 InHeight) const;

	float SampleHumidity(FIndex InWorldIndex) const;

	float SampleContinentalness(FIndex InWorldIndex) const;

	float SampleErosion(FIndex InWorldIndex) const;

	float SamplePeaksAndValleys(FIndex InWorldIndex) const;

	int32 SampleHeight(FIndex InWorldIndex, float InContinentalness, float InErosion, float InPeaksAndValleys) const;

	EVoxelBiomeType SampleBiome(const FVoxelTopography& InTopography) const;

	EVoxelRegionType SampleRegion(const FVoxelTopography& InTopography) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TemperatureSeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 HumiditySeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ClimateOctaves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ClimateScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVoxelSurfaceNoiseLayer> HeightLayers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ContinentScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ErosionScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RidgeScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ContinentHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MountainHeight;
};
