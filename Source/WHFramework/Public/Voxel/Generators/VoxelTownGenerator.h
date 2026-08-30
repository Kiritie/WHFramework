
#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerator.h"
#include "Math/MathTypes.h"
#include "Tool/PathFinder.h"
#include "VoxelTownGenerator.generated.h"

class UVoxelPrefabData;
/** 体素小镇生成器 */
UCLASS(BlueprintType)
class WHFRAMEWORK_API UVoxelTownGenerator : public UVoxelGenerator
{
	GENERATED_BODY()

public:
	UVoxelTownGenerator();

public:
	virtual void Initialize(UVoxelModule* InModule) override;
	
	virtual void Generate(UVoxelChunk* InChunk) override;

public:
	void DevelopeDomains(FIndex InAnchorChunkIndex);

	void PlaceBuildings(FIndex InAnchorChunkIndex);

	void PlacePaths();

	//检查是否有障碍物（不可走）
	bool InBarrier(FVector2D InPos);

	//计算权值公式
	TPair<float, float> WeightFormula(FVector2D InStartPos, FVector2D InEndPos, float InCost);

	bool PlaceOneBuilding(int32 InX, int32 InY, int32 InIndex, int32 InRotate);

	void PlanTown(FIndex InAnchorChunkIndex);

	void ApplyTownSlice(UVoxelChunk* InChunk, const TMap<FIndex, FVoxelItem>& InPlan) const;

	void SetPlannedVoxel(FIndex InWorldIndex, const FVoxelItem& InVoxelItem);

	int32 SamplePlannedHeight(FIndex InWorldIndex);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Seed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SpawnRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 InfluenceRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FPrimaryAssetId> PrefabAssets;

	FPathFinder PathFinder;
	
private:
	//发展域初始中心点
	FVector2D _StartPoint;
	//发展域
	TSet<uint64> _Domains;
	//已存在道路
	TSet<uint64> _Roads;
	//所有建筑门口点
	TArray<FVector2D> _BuildingPos;
	//所有建筑预制体
	TArray<UVoxelPrefabData*> _PrefabAssets;
	//世界坐标下的确定性小镇规划，生成时仅写入当前区块切片
	TMap<FIndex, FVoxelItem> _PlannedVoxels;
	TMap<FIndex, TSharedPtr<TMap<FIndex, FVoxelItem>>> _TownPlanCache;
	TArray<FIndex> _TownPlanCacheOrder;
	TMap<FIndex, int32> _TopographyHeightCache;
	mutable FRWLock _TownPlanCacheLock;
};
