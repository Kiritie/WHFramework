#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelViewTypes.h"

struct FVoxelStreamingSource;
struct FVoxelViewSettings;

// 体积地形的唯一空间分区；父节点仅在完整子分区就绪后退出。
struct WHFRAMEWORK_API FVoxelTerrainViewPlan
{
	TSet<FVoxelViewKey> Roots;
	TSet<FVoxelViewKey> Leaves;
	TSet<FVoxelViewKey> Required;
	// 精细区域交接依赖的完整兄弟组，必须先于无关远景构建。
	TSet<FVoxelViewKey> FineDependencies;
	int32 OverBudgetLeaves = 0;
	bool bBudgetLimited = false;

	void Build(TConstArrayView<FVoxelStreamingSource> InSources,
		const TSet<FIntVector>& InFineSections, const TSet<FVoxelViewKey>& InRoots,
		const FVoxelViewSettings& InSettings, int32 InMaximumLeaves);
	void ResolveVisible(TFunctionRef<bool(const FVoxelViewKey&)> InIsReady,
		TSet<FVoxelViewKey>& OutVisible, const TSet<FVoxelViewKey>* InPrevious = nullptr,
		const TSet<FVoxelViewKey>* InReadyNodes = nullptr,
		const TSet<FVoxelViewKey>* InReadyBranches = nullptr) const;
	bool Validate(FString& OutError) const;

private:
	bool ResolveNode(const FVoxelViewKey& InNode,
		TFunctionRef<bool(const FVoxelViewKey&)> InIsReady,
		const TSet<FVoxelViewKey>& InPreviousAncestors, const TSet<FVoxelViewKey>* InPrevious,
		const TSet<FVoxelViewKey>* InReadyBranches, TArray<FVoxelViewKey>& OutVisible) const;
};
