#pragma once

#include "CoreMinimal.h"

#include "Voxel/Generation/VoxelFeature.h"

#include "VoxelBuiltinFeatures.generated.h"

class UVoxelData;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelOreVeinFeatureConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UVoxelData> Ore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TSoftObjectPtr<UVoxelData>> Hosts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 MinLength = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 MaxLength = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 Radius = 1;
};

struct WHFRAMEWORK_API FVoxelOreVeinRuntimeConfig
{
	uint16 OreSymbol = MAX_uint16;
	TArray<uint16> HostSymbols;
	int32 MinLength = 3;
	int32 MaxLength = 12;
	int32 Radius = 1;
};

class WHFRAMEWORK_API FVoxelOreVeinFeatureAlgorithm final : public IVoxelFeatureAlgorithm
{
public:
	static const FName AlgorithmId;

	virtual FName GetId() const override;
	virtual uint32 GetVersion() const override;

	virtual bool GatherReferencedBlocks(
		const FParameter& InConfiguration,
		TArray<FPrimaryAssetId>& OutBlockAssets,
		FString& OutError) const override;

	virtual bool BakeConfiguration(
		const FVoxelFeatureBakeContext& InContext,
		const FParameter& InConfiguration,
		TArray<uint8>& OutBytes,
		FString& OutError) const override;

	virtual bool Generate(
		const FVoxelFeatureQueryContext& InContext,
		TConstArrayView<uint8> InConfigBytes,
		FVoxelFeatureInstance& OutInstance,
		FString& OutError) const override;
};

namespace VoxelBuiltinFeatures
{
	WHFRAMEWORK_API bool Register(FString& OutError);
}
