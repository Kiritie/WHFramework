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
	int32 MaxZ = 0;
};

struct FVoxelBuildingPrefabCache
{
	TArray<FVoxelBuildingRotationCache> Rotations;
	int32 ClearHeight = 0;
};

struct FVoxelBuildingPlacementPlan
{
	bool bValid = false;
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
		Chance = 1.f;
		FrontDirection = ERightAngle::RA_270;
		MaxTerrainSlope = 4;
		MinHeightAboveSeaLevel = 1;
	}

	FVoxelBuildingGenerateData(const FPrimaryAssetId& InPrefabAsset, const float InChance, const ERightAngle InFrontDirection = ERightAngle::RA_270)
		: PrefabAsset(InPrefabAsset), Chance(InChance), FrontDirection(InFrontDirection), MaxTerrainSlope(4), MinHeightAboveSeaLevel(1)
	{
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowedTypes = "VoxelPrefab"))
	FPrimaryAssetId PrefabAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float Chance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ERightAngle FrontDirection;

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
	virtual void Initialize(UVoxelModule* InModule) override;

	virtual void Generate(UVoxelChunk* InChunk) override;

protected:
	int32 SelectBuildingIndex(const FVector2D& InRandomPosition) const;

	bool PlaceBuildingSlice(UVoxelChunk* InChunk, FIndex InAnchorChunkIndex, int32 InX, int32 InY, int32 InBuildingIndex);

	FVoxelBuildingPlacementPlan BuildPlacementPlan(int32 InX, int32 InY, int32 InBuildingIndex) const;

	FVoxelBuildingPlacementPlan GetOrBuildPlacementPlan(FIndex InAnchorChunkIndex, int32 InX, int32 InY, int32 InBuildingIndex);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVoxelBuildingGenerateData> GenerateDatas;

private:
	UPROPERTY(Transient)
	TArray<UVoxelPrefabData*> _PrefabAssets;

	int32 _MaxBuildingExtent = 1;
	TArray<FVoxelBuildingPrefabCache> _PrefabCaches;
	TMap<FIndex, FVoxelBuildingPlacementPlan> _BuildingPlanCache;
	TArray<FIndex> _BuildingPlanCacheOrder;
	mutable FRWLock _BuildingPlanCacheLock;
};
