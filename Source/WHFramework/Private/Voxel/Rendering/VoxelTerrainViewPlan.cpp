#include "Voxel/Rendering/VoxelTerrainViewPlan.h"

bool FVoxelTerrainViewPlan::ResolveNode(const FVoxelViewKey& InNode,
	TFunctionRef<bool(const FVoxelViewKey&)> InIsReady, const TSet<FVoxelViewKey>& InPreviousAncestors,
	const TSet<FVoxelViewKey>* InPrevious, const TSet<FVoxelViewKey>* InReadyBranches,
	TArray<FVoxelViewKey>& OutVisible) const
{
	if (InReadyBranches && !InReadyBranches->Contains(InNode) &&
		!InPreviousAncestors.Contains(InNode) && (!InPrevious || !InPrevious->Contains(InNode)))
	{
		return false;
	}
	const bool bDesiredLeaf = Leaves.Contains(InNode);
	if (bDesiredLeaf && InIsReady(InNode))
	{
		OutVisible.Add(InNode);
		return true;
	}
	const int32 FirstChild = OutVisible.Num();
	if (InNode.Level > 0 && ((!bDesiredLeaf && Required.Contains(InNode)) || InPreviousAncestors.Contains(InNode)))
	{
		bool bChildrenReady = true;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const FVoxelViewKey Child { InNode.Coordinate * 2 + FIntVector(Index & 1, (Index >> 1) & 1, (Index >> 2) & 1), static_cast<uint8>(InNode.Level - 1) };
			bChildrenReady &= ResolveNode(Child, InIsReady, InPreviousAncestors, InPrevious, InReadyBranches, OutVisible);
		}
		if (bChildrenReady)
		{
			return true;
		}
	}
	// 初次加载也用已就绪的粗父节点补齐未完成的子节点；完整子族就绪后再整体交接。
	if (InIsReady(InNode))
	{
		OutVisible.SetNum(FirstChild, EAllowShrinking::No);
		OutVisible.Add(InNode);
		return true;
	}
	return false;
}

void FVoxelTerrainViewPlan::ResolveVisible(TFunctionRef<bool(const FVoxelViewKey&)> InIsReady,
	TSet<FVoxelViewKey>& OutVisible, const TSet<FVoxelViewKey>* InPrevious,
	const TSet<FVoxelViewKey>* InReadyNodes, const TSet<FVoxelViewKey>* InReadyBranches,
	const TSet<FVoxelViewKey>* InResolveRoots) const
{
	const TSet<FVoxelViewKey>& EffectiveRoots = InResolveRoots ? *InResolveRoots : Roots;
	TSet<FVoxelViewKey> PreviousAncestors;
	if (InPrevious)
	{
		uint8 RootLevel = 0;
		for (const FVoxelViewKey& Root : EffectiveRoots)
		{
			RootLevel = FMath::Max(RootLevel, Root.Level);
		}
		for (FVoxelViewKey Node : *InPrevious)
		{
			while (!EffectiveRoots.Contains(Node) && Node.Level < RootLevel)
			{
				Node = Node.GetParent();
				bool bAlreadyPresent = false;
				PreviousAncestors.Add(Node, &bAlreadyPresent);
				if (bAlreadyPresent) break;
			}
		}
	}
	TSet<FVoxelViewKey> BuiltReadyBranches;
	if (InReadyNodes && !InReadyBranches)
	{
		for (FVoxelViewKey Node : *InReadyNodes)
		{
			while (Node.Level < 24)
			{
				bool bAlreadyPresent = false;
				BuiltReadyBranches.Add(Node, &bAlreadyPresent);
				if (bAlreadyPresent || EffectiveRoots.Contains(Node)) break;
				Node = Node.GetParent();
			}
		}
		InReadyBranches = &BuiltReadyBranches;
	}
	// 上一已提交分区包含合法空节点；没有Actor不等于失去已发布的空间所有权。
	auto IsAvailable = [&InIsReady, InPrevious](const FVoxelViewKey& Key)
	{
		return InIsReady(Key) || (InPrevious && InPrevious->Contains(Key));
	};
	OutVisible.Reset();
	for (const FVoxelViewKey& Root : EffectiveRoots)
	{
		TArray<FVoxelViewKey> Visible;
		ResolveNode(Root, IsAvailable, PreviousAncestors, InPrevious, InReadyBranches, Visible);
		for (const FVoxelViewKey& Node : Visible)
		{
			OutVisible.Add(Node);
		}
	}
}

bool FVoxelTerrainViewPlan::Validate(FString& OutError) const
{
	TSet<FVoxelViewKey> Covered = Leaves;
	for (const FVoxelViewKey& Leaf : Leaves)
	{
		FVoxelViewKey Node = Leaf;
		while (!Roots.Contains(Node))
		{
			if (!Required.Contains(Node) || Node.Level >= 24)
			{
				OutError = TEXT("Terrain leaf has no planned root");
				return false;
			}
			Node = Node.GetParent();
			if (Leaves.Contains(Node))
			{
				OutError = TEXT("Terrain parent and descendant both own visible space");
				return false;
			}
		}
	}
	bool bChanged = true;
	while (bChanged)
	{
		bChanged = false;
		TSet<FVoxelViewKey> Parents;
		for (const FVoxelViewKey& Node : Covered)
		{
			if (!Roots.Contains(Node))
			{
				Parents.Add(Node.GetParent());
			}
		}
		for (const FVoxelViewKey& Parent : Parents)
		{
			TArray<FVoxelViewKey> Children;
			Parent.GetChildren(Children);
			if (Children.ContainsByPredicate([&Covered](const FVoxelViewKey& Child) { return !Covered.Contains(Child); }))
			{
				continue;
			}
			for (const FVoxelViewKey& Child : Children)
			{
				Covered.Remove(Child);
			}
			Covered.Add(Parent);
			bChanged = true;
		}
	}
	if (Covered.Num() != Roots.Num() || Covered.Difference(Roots).Num() != 0)
	{
		OutError = TEXT("Terrain partition contains uncovered space");
		return false;
	}
	OutError.Reset();
	return true;
}

FVoxelGameThreadContinuation FVoxelTerrainViewPlan::ResolveNodeSteps(const FVoxelViewKey InNode, TFunction<bool(const FVoxelViewKey&)> InIsReady,
	const TSet<FVoxelViewKey>& InPreviousAncestors, const TSet<FVoxelViewKey>* InPrevious, const TSet<FVoxelViewKey>* InReadyBranches,
	TArray<FVoxelViewKey>& OutVisible, bool& bOutReady) const
{
	co_await FVoxelGameThreadContinuation::Yield();
	if (InReadyBranches && !InReadyBranches->Contains(InNode) && !InPreviousAncestors.Contains(InNode) && (!InPrevious || !InPrevious->Contains(InNode)))
	{
		bOutReady = false;
		co_return;
	}
	const bool bDesiredLeaf = Leaves.Contains(InNode);
	if (bDesiredLeaf && InIsReady(InNode))
	{
		OutVisible.Add(InNode);
		bOutReady = true;
		co_return;
	}
	const int32 FirstChild = OutVisible.Num();
	if (InNode.Level > 0 && ((!bDesiredLeaf && Required.Contains(InNode)) || InPreviousAncestors.Contains(InNode)))
	{
		bool bChildrenReady = true;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			const FVoxelViewKey Child{InNode.Coordinate * 2 + FIntVector(Index & 1, (Index >> 1) & 1, (Index >> 2) & 1), static_cast<uint8>(InNode.Level - 1)};
			bool bChildReady = false;
			co_await ResolveNodeSteps(Child, InIsReady, InPreviousAncestors, InPrevious, InReadyBranches, OutVisible, bChildReady);
			bChildrenReady &= bChildReady;
		}
		if (bChildrenReady)
		{
			bOutReady = true;
			co_return;
		}
	}
	// 初次加载也用已就绪的粗父节点补齐未完成的子节点；完整子族就绪后再整体交接。
	if (InIsReady(InNode))
	{
		OutVisible.SetNum(FirstChild, EAllowShrinking::No);
		OutVisible.Add(InNode);
		bOutReady = true;
		co_return;
	}
	bOutReady = false;
	co_return;
}

FVoxelGameThreadContinuation FVoxelTerrainViewPlan::ResolveVisibleSteps(TFunction<bool(const FVoxelViewKey&)> InIsReady, TSet<FVoxelViewKey>& OutVisible,
	const TSet<FVoxelViewKey>* InPrevious, const TSet<FVoxelViewKey>* InReadyBranches, const TSet<FVoxelViewKey>* InResolveRoots) const
{
	const TSet<FVoxelViewKey>& EffectiveRoots = InResolveRoots ? *InResolveRoots : Roots;
	TSet<FVoxelViewKey> PreviousAncestors;
	if (InPrevious)
	{
		uint8 RootLevel = 0;
		for (const FVoxelViewKey& Root : EffectiveRoots)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			RootLevel = FMath::Max(RootLevel, Root.Level);
		}
		for (FVoxelViewKey Node : *InPrevious)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			while (!EffectiveRoots.Contains(Node) && Node.Level < RootLevel)
			{
				co_await FVoxelGameThreadContinuation::Yield();
				Node = Node.GetParent();
				bool bAlreadyPresent = false;
				PreviousAncestors.Add(Node, &bAlreadyPresent);
				if (bAlreadyPresent)
					break;
			}
		}
	}
	// 上一已提交分区包含合法空节点；没有Actor不等于失去已发布的空间所有权。
	auto IsAvailable = [&InIsReady, InPrevious](const FVoxelViewKey& Key)
	{
		return InIsReady(Key) || (InPrevious && InPrevious->Contains(Key));
	};
	OutVisible.Reset();
	for (const FVoxelViewKey& Root : EffectiveRoots)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		TArray<FVoxelViewKey> Visible;
		bool bReady = false;
		co_await ResolveNodeSteps(Root, IsAvailable, PreviousAncestors, InPrevious, InReadyBranches, Visible, bReady);
		for (const FVoxelViewKey& Node : Visible)
		{
			co_await FVoxelGameThreadContinuation::Yield();
			OutVisible.Add(Node);
		}
	}
}
