#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"
#include "VoxelGenerationRecipe.generated.h"

UENUM(BlueprintType)
enum class EVoxelStructureAdaptation : uint8
{
	None = 0,
	Flatten,
	CutFill,
	Bury,
	Blend,
	Encapsulate
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelFeaturePlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Spacing = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ChancePermille = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinZ = MIN_int32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxZ = MAX_int32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxSlopePermille = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRequireSurface = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRequireSolidFloor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAllowNearWater = true;

	bool Validate(FString& OutError) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelStructurePlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Spacing = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Separation = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ChancePermille = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinZ = MIN_int32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxZ = MAX_int32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxSlopePermille = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRequireSurface = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxTerrainAdjustment = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 BlendRadius = 8;

	bool Validate(FString& OutError) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelStructureConnectorData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntVector Position = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "3"))
	uint8 Yaw = 0;
};

struct WHFRAMEWORK_API FVoxelGenerationPalette
{
	uint16 Air = 0;
	uint16 Stone = MAX_uint16;
	uint16 Dirt = MAX_uint16;
	uint16 Grass = MAX_uint16;
	uint16 Sand = MAX_uint16;
	uint16 Snow = MAX_uint16;
	uint16 Water = MAX_uint16;
	uint16 Lava = MAX_uint16;
	uint16 Bedrock = MAX_uint16;
	uint16 Road = MAX_uint16;

	bool Validate(int32 InBlockCount, FString& OutError) const;
};

struct WHFRAMEWORK_API FVoxelEcologyRuntimePalette
{
	uint16 TreeTrunk = MAX_uint16;
	uint16 TreeLeaves = MAX_uint16;
	uint16 GrassPlant = MAX_uint16;

	bool Validate(
		int32 InBlockCount,
		const FVoxelEcologyGenerationSettings& InSettings,
		FString& OutError) const;
};

struct WHFRAMEWORK_API FVoxelSurfaceRuntimeRule
{
	FVoxelGenerationRange Height;
	FVoxelGenerationRange Slope;
	FVoxelGenerationRange Temperature;
	FVoxelGenerationRange Moisture;
	int32 MinDepth = 0;
	int32 MaxDepth = 0;
	uint16 BlockSymbol = MAX_uint16;
	bool bRiverOnly = false;
	bool bLakeOnly = false;
	bool bOceanOnly = false;
	bool bCoastOnly = false;

	bool Validate(int32 InBlockCount, FString& OutError) const;
};

struct WHFRAMEWORK_API FVoxelSurfaceRuntimeRuleSet
{
	FName StableId;
	TArray<FVoxelSurfaceRuntimeRule> Rules;

	bool Validate(int32 InBlockCount, FString& OutError) const;
};

struct WHFRAMEWORK_API FVoxelBiomeRuntimeDefinition
{
	FName StableId;
	uint64 StableHash = 0;
	int32 Priority = 0;
	FVoxelGenerationRange Temperature;
	FVoxelGenerationRange Moisture;
	FVoxelGenerationRange Continentalness;
	FVoxelGenerationRange Erosion;
	FVoxelGenerationRange Height;
	FVoxelGenerationRange Slope;
	int32 SurfaceRuleIndex = INDEX_NONE;
	TArray<int32> FeatureIndices;
	TArray<int32> StructureIndices;

	bool Validate(int32 InSurfaceRuleCount, int32 InFeatureCount, int32 InStructureCount, FString& OutError) const;
};

struct WHFRAMEWORK_API FVoxelFeatureRuntimeDefinition
{
	FName StableId;
	uint64 StableHash = 0;
	FName AlgorithmId;
	uint32 AlgorithmVersion = 0;
	EVoxelGenerationStage Stage = EVoxelGenerationStage::None;
	FVoxelFeaturePlacement Placement;
	TArray<uint8> ConfigBytes;

	bool Validate(FString& OutError) const;
};

struct WHFRAMEWORK_API FVoxelStructureRun
{
	FIntVector Start = FIntVector::ZeroValue;
	int32 Length = 0;
	uint32 Value = 0;

	bool Validate(int32 InBlockCount, FString& OutError) const;
};

struct WHFRAMEWORK_API FVoxelStructureDetailRuntimeSocket
{
	FName DetailId;
	FIntVector SourceCorner = FIntVector::ZeroValue;
	uint8 Yaw = 0;
};

struct WHFRAMEWORK_API FVoxelStructurePieceTemplate
{
	FName StableId;
	int32 SourceCellCentimeters = 100;
	int32 GroundZ = 0;
	FVoxelGenerationBounds Bounds;
	FIntVector Entrance = FIntVector::ZeroValue;
	TArray<FVoxelStructureRun> Writes;
	TArray<FVoxelGenerationBounds> ClearVolumes;
	TArray<FVoxelStructureConnectorData> Connectors;
	TArray<FVoxelStructureDetailRuntimeSocket> Details;

	bool Validate(int32 InBlockCount, FString& OutError) const;
};

struct WHFRAMEWORK_API FVoxelStructureRuntimeDefinition
{
	FName StableId;
	uint64 StableHash = 0;
	EVoxelGenerationStage Stage = EVoxelGenerationStage::None;
	FVoxelStructurePlacement Placement;
	EVoxelStructureAdaptation Adaptation = EVoxelStructureAdaptation::None;
	FName LayoutAlgorithmId;
	uint32 LayoutAlgorithmVersion = 0;
	TArray<FVoxelStructurePieceTemplate> Pieces;

	bool Validate(int32 InBlockCount, FString& OutError) const;
};

struct WHFRAMEWORK_API FVoxelGenerationRecipe
{
	static constexpr uint32 CurrentAlgorithmVersion = 5;

	uint32 AlgorithmVersion = CurrentAlgorithmVersion;
	uint64 RecipeHash = 0;
	int32 CellCentimeters = 25;
	FVoxelGenerationSettings Settings;
	TArray<FName> BlockNames;
	FVoxelGenerationPalette Palette;
	FVoxelEcologyRuntimePalette Ecology;
	TArray<FVoxelBiomeRuntimeDefinition> Biomes;
	TArray<FVoxelFeatureRuntimeDefinition> Features;
	TArray<FVoxelStructureRuntimeDefinition> Structures;
	TArray<FVoxelSurfaceRuntimeRuleSet> SurfaceRules;
	TMap<FName, int32> BlockLookup;
	TMap<FName, int32> BiomeLookup;
	TMap<FName, int32> FeatureLookup;
	TMap<FName, int32> StructureLookup;
	TMap<FName, int32> SurfaceRuleLookup;

	bool Validate(FString& OutError) const;
	bool BuildLookups(FString& OutError);
	int32 FindBlockSymbol(FName InStableName) const;
	int32 FindBiome(FName InStableId) const;
	int32 FindFeature(FName InStableId) const;
	int32 FindStructure(FName InStableId) const;
	int32 FindSurfaceRules(FName InStableId) const;
};
