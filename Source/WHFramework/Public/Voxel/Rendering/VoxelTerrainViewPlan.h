#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Rendering/VoxelGameThreadContinuation.h"

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

	bool Build(TConstArrayView<FVoxelStreamingSource> InSources,
		const TSet<FIntVector>& InFineSections, const TSet<FVoxelViewKey>& InRoots,
		const FVoxelViewSettings& InSettings, FString& OutError);
	void ResolveVisible(TFunctionRef<bool(const FVoxelViewKey&)> InIsReady,
		TSet<FVoxelViewKey>& OutVisible, const TSet<FVoxelViewKey>* InPrevious = nullptr,
		const TSet<FVoxelViewKey>* InReadyNodes = nullptr,
		const TSet<FVoxelViewKey>* InReadyBranches = nullptr,
		const TSet<FVoxelViewKey>* InResolveRoots = nullptr) const;
	FVoxelGameThreadContinuation ResolveVisibleSteps(TFunction<bool(const FVoxelViewKey&)> InIsReady, TSet<FVoxelViewKey>& OutVisible,
		const TSet<FVoxelViewKey>* InPrevious, const TSet<FVoxelViewKey>* InReadyBranches, const TSet<FVoxelViewKey>* InResolveRoots) const;
	bool Validate(FString& OutError) const;

private:
	FVoxelGameThreadContinuation ResolveNodeSteps(FVoxelViewKey InNode, TFunction<bool(const FVoxelViewKey&)> InIsReady,
		const TSet<FVoxelViewKey>& InPreviousAncestors, const TSet<FVoxelViewKey>* InPrevious, const TSet<FVoxelViewKey>* InReadyBranches,
		TArray<FVoxelViewKey>& OutVisible, bool& bOutReady) const;
	bool BuildInternal(TConstArrayView<FVoxelStreamingSource> InSources, const TSet<FIntVector>& InFineSections, const TSet<FVoxelViewKey>& InRoots,
		const FVoxelViewSettings& InSettings, FString& OutError);

	bool ResolveNode(const FVoxelViewKey& InNode, TFunctionRef<bool(const FVoxelViewKey&)> InIsReady, const TSet<FVoxelViewKey>& InPreviousAncestors,
		const TSet<FVoxelViewKey>* InPrevious, const TSet<FVoxelViewKey>* InReadyBranches, TArray<FVoxelViewKey>& OutVisible) const;
};
