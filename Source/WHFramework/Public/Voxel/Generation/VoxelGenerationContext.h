#pragma once
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "VoxelGenerationContext.generated.h"
UENUM(BlueprintType)
enum class EVoxelBiomeId : uint8
{
	None,
	Plains,
	Forest,
	Desert,
	Snow,
	Mountain,
	Ocean
};
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelGenerationSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinZ = -64;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxZ = 320;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SeaLevel = 62;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 BaseHeight = 64;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ContinentalPeriod = 1250;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ErosionPeriod = 667;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MountainPeriod = 400;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ClimatePeriod = 2100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 DetailPeriod = 40;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ContinentalAmplitude = 40;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MountainAmplitude = 80;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 DetailAmplitude = 5;
	bool Validate(FString& Error) const;
};
struct WHFRAMEWORK_API FVoxelGenerationRuntimeConfig
{
	FVoxelGenerationSettings Settings;
	FVoxelTypeId Stone = 0, Dirt = 0, Grass = 0, Sand = 0, Snow = 0, Water = 0;
};
struct WHFRAMEWORK_API FVoxelColumnSample
{
	int64 HeightQ16 = 0;
	int32 SurfaceZ = 0;
	EVoxelBiomeId Biome = EVoxelBiomeId::Plains;
};
struct WHFRAMEWORK_API FVoxelWorldManifest
{
	static constexpr uint32 ProtocolVersion = 2;
	static constexpr uint32 GeneratorVersion = 2;
	FGuid WorldId;
	FVoxelGenerationSettings Settings;
	int32 BlockSizeCentimeters = 100;
	uint64 RegistryHash = 0;
	uint64 RecipeHash = 0;
	uint64 BaseSampleHash = 0;
};
