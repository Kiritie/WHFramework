#pragma once

#include "CoreMinimal.h"
#include "Math/MathTypes.h"
#include "VoxelGenerator.h"
#include "VoxelBuildingGenerator.generated.h"

class UVoxelPrefabData;

struct FVoxelBuildingRotationCache
{
	TMap<FIndex, TArray<FVoxelItem>> ChunkSlices;
	TArray<FIndex> EntranceIndices;
	int32 MinX = 0;
	int32 MaxX = -1;
	int32 MinY = 0;
	int32 MaxY = -1;
	int32 MinZ = 0;
	int32 MaxZ = 0;
};

struct FVoxelBuildingPrefabCache
{
	TArray<FVoxelBuildingRotationCache> Rotations;
	int32 ClearHeight = 0;
	int32 GroundOffset = 0;
	int32 Extent = 1;
};

struct FVoxelBuildingPlacementPlan
{
	bool bValid = false;
	FIndex AnchorChunkIndex;
	int32 Rotation = INDEX_NONE;
	int32 GroundHeight = 0;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FVoxelBuildingGenerateData
{
	GENERATED_BODY()

public:
	FVoxelBuildingGenerateData()
	{
		PrefabAsset = FPrimaryAssetId();
		SpawnRange = 1024.f;
		SpawnChance = 1.f;
		bAllowTerrainAdaptation = false;
		MaxTerrainSlope = 4;
		MinHeightAboveSeaLevel = 1;
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowedTypes = "VoxelPrefab"))
	FPrimaryAssetId PrefabAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0", ToolTip = "每个生成分区的边长，单位为格，按区块大小向上对齐；每种建筑每个分区最多一座"))
	float SpawnRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "没有合适陆地时允许整平地面或抬高地基；概率为 1 时每个分区都有建筑"))
	bool bAllowTerrainAdaptation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 MaxTerrainSlope;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 MinHeightAboveSeaLevel;
};

/** 体素建筑生成器 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelBuildingGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelBuildingGenerator();

public:
	virtual void Initialize(UVoxelModule* InModule, int32 InStage) override;

	virtual void Generate(UVoxelChunk* InChunk) override;

protected:
	bool PlaceBuildingSlice(UVoxelChunk* InChunk, int32 InBuildingIndex, const FVoxelBuildingPlacementPlan& InPlan);

	FVoxelBuildingPlacementPlan BuildPlacementPlan(int32 InX, int32 InY, int32 InBuildingIndex, bool bInAdaptTerrain) const;

	FVoxelBuildingPlacementPlan GetOrBuildPlacementPlan(FIndex InCellIndex, int32 InBuildingIndex);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVoxelBuildingGenerateData> GenerateDatas;

private:
	UPROPERTY(Transient)
	TArray<UVoxelPrefabData*> _PrefabAssets;

	TArray<FVoxelBuildingPrefabCache> _PrefabCaches;
	TMap<FIntVector4, FVoxelBuildingPlacementPlan> _BuildingPlanCache;
	TArray<FIntVector4> _BuildingPlanCacheOrder;
	mutable FRWLock _BuildingPlanCacheLock;
};
