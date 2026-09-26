#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerationTypes.generated.h"

FORCEINLINE uint32 GetTypeHash(const FIntVector& InValue)
{
	return HashCombineFast(
		HashCombineFast(::GetTypeHash(InValue.X), ::GetTypeHash(InValue.Y)),
		::GetTypeHash(InValue.Z));
}

FORCEINLINE uint32 GetTypeHash(const FIntPoint& InValue)
{
	return HashCombineFast(::GetTypeHash(InValue.X), ::GetTypeHash(InValue.Y));
}

UENUM(BlueprintType)
enum class EVoxelGenerationStage : uint8
{
	None = 0,
	Climate,
	Biome,
	StructurePlanning,
	HydrologyPlanning,
	CavePlanning,
	TerrainDensity,
	Aquifer,
	Surface,
	Carving,
	HydrologyRaster,
	BaseEcology,
	UndergroundStructures,
	SurfaceStructures,
	UndergroundOres,
	UndergroundDecoration,
	FluidFeatures,
	Vegetation,
	SurfaceDecoration,
	TopLayer
};

UENUM(BlueprintType)
enum class EVoxelFluidKind : uint8
{
	None = 0,
	Water,
	Lava
};

UENUM(BlueprintType)
enum class EVoxelLandformKind : uint8
{
	None = 0,
	Plain,
	RollingHills,
	Highland,
	MountainRange,
	Plateau,
	Basin
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelLandformGenerationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "512"))
	int32 DomainPeriod = 24000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "512"))
	int32 ReliefPeriod = 8000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "512"))
	int32 HillsPeriod = 5000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "512"))
	int32 PlateauPeriod = 12000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 DomainWarpCells = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 PlainRelief = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 HillRelief = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 HighlandUplift = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 PlateauUplift = 320;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 BasinDepth = 160;

	bool Validate(FString& OutError) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelGenerationRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation")
	int32 Min = MIN_int32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation")
	int32 Max = MAX_int32;

	bool Contains(int32 InValue) const;
	bool Validate(FString& OutError) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelBiomeEcologyModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Biome|Ecology")
	bool bAllowTrees = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Biome|Ecology")
	bool bAllowGrass = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Biome|Ecology")
	bool bAllowFlowers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Biome|Ecology", meta = (ClampMin = "0", ClampMax = "2000"))
	int32 TreeDensityScalePermille = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Biome|Ecology", meta = (ClampMin = "0", ClampMax = "2000"))
	int32 GrassDensityScalePermille = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Biome|Ecology", meta = (ClampMin = "0", ClampMax = "2000"))
	int32 FlowerDensityScalePermille = 1000;

	bool Validate(FString& OutError) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelTreeGenerationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Tree")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Tree", meta = (ClampMin = "1"))
	int32 MinHeight = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Tree", meta = (ClampMin = "1"))
	int32 MaxHeight = 28;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Tree", meta = (ClampMin = "4"))
	int32 Spacing = 48;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Tree", meta = (ClampMin = "0", ClampMax = "2000"))
	int32 DensityPermille = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Tree", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 ChancePermille = 650;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Tree", meta = (ClampMin = "1"))
	int32 CrownRadius = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Tree", meta = (ClampMin = "0"))
	int32 MaxSlopePermille = 400;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Tree")
	FVoxelGenerationRange Temperature = {-16384, 32767};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Tree")
	FVoxelGenerationRange Moisture = {-4096, 32767};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Tree")
	bool bAllowNearWater = false;

	bool Validate(FString& OutError) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelGrassGenerationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Grass")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Grass", meta = (ClampMin = "2"))
	int32 Spacing = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Grass", meta = (ClampMin = "0", ClampMax = "2000"))
	int32 DensityPermille = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Grass", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 ChancePermille = 850;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Grass", meta = (ClampMin = "0"))
	int32 PatchRadius = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Grass", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 PatchFillPermille = 650;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Grass", meta = (ClampMin = "0"))
	int32 MaxSlopePermille = 650;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Grass")
	FVoxelGenerationRange Temperature = {-24576, 32767};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Grass")
	FVoxelGenerationRange Moisture = {-16384, 32767};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Grass")
	bool bAllowNearWater = true;

	bool Validate(FString& OutError) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelFlowerGenerationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Flower")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Flower", meta = (ClampMin = "2"))
	int32 Spacing = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Flower", meta = (ClampMin = "0", ClampMax = "2000"))
	int32 DensityPermille = 700;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Flower", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 ChancePermille = 550;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Flower", meta = (ClampMin = "0"))
	int32 PatchRadius = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Flower", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 PatchFillPermille = 350;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Flower", meta = (ClampMin = "0"))
	int32 MaxSlopePermille = 450;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Flower")
	FVoxelGenerationRange Temperature = {-16384, 32767};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology|Flower")
	FVoxelGenerationRange Moisture = {-8192, 32767};

	bool Validate(FString& OutError) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelEcologyGenerationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology")
	FVoxelTreeGenerationSettings Tree;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology")
	FVoxelGrassGenerationSettings Grass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology")
	FVoxelFlowerGenerationSettings Flower;

	bool Validate(FString& OutError) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelGenerationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|World")
	int32 Seed = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|World")
	int32 MinZ = -512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|World")
	int32 MaxZ = 2048;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|World")
	int32 SeaLevel = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|World")
	int32 BaseHeight = 80;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Terrain", meta = (ClampMin = "1"))
	int32 ContinentalPeriod = 16000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Terrain", meta = (ClampMin = "1"))
	int32 ErosionPeriod = 8000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Terrain", meta = (ClampMin = "1"))
	int32 MountainPeriod = 4000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Climate", meta = (ClampMin = "1"))
	int32 ClimatePeriod = 24000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Terrain", meta = (ClampMin = "1"))
	int32 DetailPeriod = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Terrain", meta = (ClampMin = "0"))
	int32 ContinentalAmplitude = 384;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Terrain", meta = (ClampMin = "0"))
	int32 MountainAmplitude = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Terrain", meta = (ClampMin = "0"))
	int32 DetailAmplitude = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Terrain")
	FVoxelLandformGenerationSettings Landform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "1"))
	int32 HydrologyCellSize = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "8"))
	int32 HydrologyRegionSide = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "1"))
	int32 RiverSourceAccumulation = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "1"))
	int32 RiverBaseHalfWidth = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "1"))
	int32 RiverBaseDepth = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "0"))
	int32 RiverWidthGrowthPerLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "0"))
	int32 RiverDepthGrowthPerLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "0", ClampMax = "8"))
	int32 RiverMaxGrowthLevels = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "1"))
	int32 RiverBankWidth = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "1"))
	int32 RiverShoreWidth = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "0", ClampMax = "2"))
	int32 RiverShapeSmoothingPasses = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "0"))
	int32 RiverMeanderStrength = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "8"))
	int32 RiverMeanderFrequency = 96;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "1", ClampMax = "4"))
	int32 RiverMeanderOctaves = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "1", ClampMax = "80"))
	int32 RiverMaxMeanderAngle = 35;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "8"))
	int32 HydrologyHaloCells = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "8"))
	int32 HydrologySinkSpacing = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "1"))
	int32 RiverSourceSpacing = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "128"))
	int32 RiverTraceBudget = 8192;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Hydrology", meta = (ClampMin = "64"))
	int32 LakeMaxCells = 32768;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Cave", meta = (ClampMin = "32"))
	int32 CaveSpacing = 384;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Cave", meta = (ClampMin = "1"))
	int32 CaveMinDepth = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Cave", meta = (ClampMin = "1"))
	int32 CaveMaxDepth = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Cave", meta = (ClampMin = "2"))
	int32 CaveMainRadius = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Cave", meta = (ClampMin = "2"))
	int32 CaveBranchRadius = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Cave", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 CaveSystemChancePermille = 500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Cave", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 CaveEntranceChancePermille = 80;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Cave", meta = (ClampMin = "4", ClampMax = "64"))
	int32 CaveEntranceLength = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Cave", meta = (ClampMin = "1", ClampMax = "4"))
	int32 CaveEntranceDropPerStep = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Cave", meta = (ClampMin = "4", ClampMax = "64"))
	int32 CaveEntranceTransitionDepth = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Cave", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 CaveRoomChancePermille = 60;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Cave", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 CaveBranchChancePermille = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Aquifer", meta = (ClampMin = "1"))
	int32 AquiferSpacing = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Aquifer", meta = (ClampMin = "1"))
	int32 AquiferRadius = 28;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Aquifer")
	int32 LavaCeiling = -320;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Generation|Ecology")
	FVoxelEcologyGenerationSettings Ecology;

	bool Validate(FString& OutError) const;
};

struct WHFRAMEWORK_API FVoxelGenerationBounds
{
	FIntVector Min = FIntVector::ZeroValue;
	FIntVector Max = FIntVector::ZeroValue;

	bool IsValid() const;
	bool Contains(const FIntVector& InPosition) const;
	bool Intersects(const FVoxelGenerationBounds& InOther) const;
	FVoxelGenerationBounds Expand(int32 InCells) const;
};

struct WHFRAMEWORK_API FVoxelStableId
{
	uint64 High = 0;
	uint64 Low = 0;

	bool IsValid() const;
	bool operator==(const FVoxelStableId& InOther) const;
	bool operator!=(const FVoxelStableId& InOther) const;
	bool operator<(const FVoxelStableId& InOther) const;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelStableId& InId)
{
	return HashCombineFast(::GetTypeHash(InId.High), ::GetTypeHash(InId.Low));
}

struct WHFRAMEWORK_API FVoxelClimateSample
{
	int32 TemperatureQ15 = 0;
	int32 MoistureQ15 = 0;
	int32 ContinentalnessQ15 = 0;
	int32 ErosionQ15 = 0;
	int32 RidgeQ15 = 0;
};

struct WHFRAMEWORK_API FVoxelLandformSample
{
	EVoxelLandformKind Dominant = EVoxelLandformKind::Plain;
	int32 PlainQ15 = 0;
	int32 HillsQ15 = 0;
	int32 HighlandQ15 = 0;
	int32 MountainQ15 = 0;
	int32 PlateauQ15 = 0;
	int32 BasinQ15 = 0;
	int32 ReliefQ15 = 0;
};

struct WHFRAMEWORK_API FVoxelMacroTerrainSample
{
	int32 SurfaceZ = 0;
	int32 DensityHeight = 0;
	int32 SlopePermille = 0;
	FVoxelLandformSample Landform;
};

UENUM(BlueprintType)
enum class EVoxelRiverSurfaceZone : uint8
{
	None = 0,
	ChannelBed,
	WetMargin,
	DryBank,
	Floodplain
};

UENUM(BlueprintType)
enum class EVoxelSurfaceRiverRule : uint8
{
	Any = 0,
	None,
	ChannelBed,
	WetMargin,
	DryBank,
	Floodplain
};

struct WHFRAMEWORK_API FVoxelColumnSample
{
	int32 SurfaceZ = 0;
	int32 DensityHeight = 0;
	int32 SlopePermille = 0;
	int32 SurfaceWaterZ = MIN_int32;
	int32 GroundWaterZ = MIN_int32;
	int32 RiverDistanceCells = MAX_int32;
	int32 BankDistanceCells = MAX_int32;
	int32 FloodplainStrengthQ15 = 0;
	FVoxelStableId RiverId;
	EVoxelRiverSurfaceZone RiverZone = EVoxelRiverSurfaceZone::None;
	uint16 BiomeIndex = MAX_uint16;
	uint16 SurfaceMaterial = 0;
	FVoxelClimateSample Climate;
	FVoxelLandformSample Landform;
	bool bRiver = false;
	bool bLake = false;
	bool bOcean = false;
	bool bCoast = false;
};

struct WHFRAMEWORK_API FVoxelEnvironmentSample
{
	FVoxelColumnSample Column;
};

struct WHFRAMEWORK_API FVoxelSurfaceCandidate
{
	FIntPoint XY = FIntPoint::ZeroValue;
	int32 GroundZ = 0;
	int32 WaterZ = MIN_int32;
	int32 SlopePermille = 0;
	uint16 BiomeIndex = MAX_uint16;
	bool bRiver = false;
	bool bLake = false;
	bool bOcean = false;
	bool bCoast = false;
	bool bValid = false;
};

struct WHFRAMEWORK_API FVoxelAquiferSample
{
	EVoxelFluidKind Fluid = EVoxelFluidKind::None;
	int32 SurfaceZ = MIN_int32;
	bool bBarrier = false;
};

struct WHFRAMEWORK_API FVoxelGenerationTileKey
{
	FIntVector Coordinate = FIntVector::ZeroValue;

	bool operator==(const FVoxelGenerationTileKey& InOther) const { return Coordinate == InOther.Coordinate; }
	bool operator<(const FVoxelGenerationTileKey& InOther) const
	{
		if (Coordinate.Z != InOther.Coordinate.Z) return Coordinate.Z < InOther.Coordinate.Z;
		if (Coordinate.Y != InOther.Coordinate.Y) return Coordinate.Y < InOther.Coordinate.Y;
		return Coordinate.X < InOther.Coordinate.X;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelGenerationTileKey& InKey)
{
	return ::GetTypeHash(InKey.Coordinate);
}

struct WHFRAMEWORK_API FVoxelEcologyTileKey
{
	FIntPoint Coordinate = FIntPoint::ZeroValue;

	bool operator==(const FVoxelEcologyTileKey& InOther) const
	{
		return Coordinate == InOther.Coordinate;
	}

	bool operator<(const FVoxelEcologyTileKey& InOther) const
	{
		if (Coordinate.Y != InOther.Coordinate.Y)
		{
			return Coordinate.Y < InOther.Coordinate.Y;
		}
		return Coordinate.X < InOther.Coordinate.X;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelEcologyTileKey& InKey)
{
	return ::GetTypeHash(InKey.Coordinate);
}

struct WHFRAMEWORK_API FVoxelHydrologyRegionKey
{
	FIntPoint Coordinate = FIntPoint::ZeroValue;

	bool operator==(const FVoxelHydrologyRegionKey& InOther) const { return Coordinate == InOther.Coordinate; }
	bool operator<(const FVoxelHydrologyRegionKey& InOther) const
	{
		if (Coordinate.Y != InOther.Coordinate.Y) return Coordinate.Y < InOther.Coordinate.Y;
		return Coordinate.X < InOther.Coordinate.X;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelHydrologyRegionKey& InKey)
{
	return ::GetTypeHash(InKey.Coordinate);
}
