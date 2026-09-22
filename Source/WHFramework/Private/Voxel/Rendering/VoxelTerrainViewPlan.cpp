#include "Voxel/Rendering/VoxelTerrainViewPlan.h"

#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

void FVoxelTerrainViewPlan::Build(TConstArrayView<FVoxelStreamingSource> InSources,
	const TSet<FIntVector>& InFineSections, const TSet<FVoxelViewKey>& InRoots,
	const FVoxelViewSettings& InSettings, const int32 InMaximumLeaves)
{
	Roots = InRoots;
	Leaves.Reset();
	Required.Reset();
	FineDependencies.Reset();
	OverBudgetLeaves = 0;
	bBudgetLimited = false;
	uint8 RootLevel = 1;
	for (const FVoxelViewKey& Root : Roots)
	{
		RootLevel = FMath::Max(RootLevel, Root.Level);
	}
	TSet<FVoxelViewKey> FineAncestors;
	for (const FIntVector& Section : InFineSections)
	{
		FVoxelViewKey Node { Section, 0 };
		while (Node.Level < RootLevel)
		{
			Node = Node.GetParent();
			FineAncestors.Add(Node);
		}
		Roots.Add(Node);
	}
	TSet<FVoxelViewKey> NormalizedRoots;
	for (FVoxelViewKey Root : Roots)
	{
		while (Root.Level < RootLevel)
		{
			Root = Root.GetParent();
		}
		NormalizedRoots.Add(Root);
	}
	Roots = MoveTemp(NormalizedRoots);
	Leaves = Roots;
	Required = Roots;
	auto Split = [this](const FVoxelViewKey& Node)
	{
		TArray<FVoxelViewKey> Children;
		Node.GetChildren(Children);
		Leaves.Remove(Node);
		for (const FVoxelViewKey& Child : Children)
		{
			Leaves.Add(Child);
			Required.Add(Child);
		}
	};
	for (int32 Level = RootLevel; Level > 0; --Level)
	{
		for (const FVoxelViewKey& Node : FineAncestors)
		{
			if (Node.Level == Level)
			{
				Split(Node);
				FineDependencies.Add(Node);
				TArray<FVoxelViewKey> Children;
				Node.GetChildren(Children);
				for (const FVoxelViewKey& Child : Children)
				{
					FineDependencies.Add(Child);
				}
			}
		}
	}
	struct FCandidate
	{
		FVoxelViewKey Key;
		double Error = 0.0;
	};
	TArray<FCandidate> Pending;
	auto HigherError = [](const FCandidate& Left, const FCandidate& Right) { return Left.Error > Right.Error; };
	auto Admit = [&](const FVoxelViewKey& Node)
	{
		if (Node.Level <= 1)
		{
			return;
		}
		const FVoxelGenerationBounds Bounds = Node.GetBounds();
		const FBox Box(FVector(Bounds.Min), FVector(Bounds.Max));
		double Error = 0.0;
		for (const FVoxelStreamingSource& Source : InSources)
		{
			if (Source.RenderMode != EVoxelStreamingRenderMode::Full)
			{
				continue;
			}
			const int32 Distance = FMath::FloorToInt(FMath::Sqrt(Box.ComputeSquaredDistanceToPoint(FVector(Source.Center))));
			const uint8 Level = FMath::Max<uint8>(1, VoxelViewLod::ResolveScreenErrorLevel(Distance, 1, Source, InSettings, RootLevel));
			if (Level < Node.Level)
			{
				Error = FMath::Max(Error, static_cast<double>(Node.GetStep()) / FMath::Max(1, Distance));
			}
		}
		if (Error > 0.0)
		{
			Pending.HeapPush(FCandidate { Node, Error }, HigherError);
		}
	};
	for (const FVoxelViewKey& Node : Leaves)
	{
		Admit(Node);
	}
	const int32 RefinementLimit = FMath::Max(0, InMaximumLeaves) + FMath::Max(0, Leaves.Num() - Roots.Num());
	while (!Pending.IsEmpty() && Leaves.Num() + 7 <= RefinementLimit)
	{
		const double MaximumError = Pending[0].Error;
		TArray<FVoxelViewKey> Candidates;
		while (!Pending.IsEmpty() && FMath::IsNearlyEqual(Pending[0].Error, MaximumError, UE_DOUBLE_SMALL_NUMBER))
		{
			FCandidate Candidate;
			Pending.HeapPop(Candidate, HigherError, EAllowShrinking::No);
			Candidates.Add(Candidate.Key);
		}
		if (Leaves.Num() + Candidates.Num() * 7 > RefinementLimit)
		{
			bBudgetLimited = true;
			break;
		}
		for (const FVoxelViewKey& Node : Candidates)
		{
			Split(Node);
			TArray<FVoxelViewKey> Children;
			Node.GetChildren(Children);
			for (const FVoxelViewKey& Child : Children)
			{
				Admit(Child);
			}
		}
	}
	bBudgetLimited |= !Pending.IsEmpty();
	TArray<FVoxelViewKey> BalanceQueue = Leaves.Array();
	for (int32 Index = 0; Index < BalanceQueue.Num(); ++Index)
	{
		const FVoxelViewKey Node = BalanceQueue[Index];
		if (!Leaves.Contains(Node))
		{
			continue;
		}
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (int32 Sign = -1; Sign <= 1; Sign += 2)
			{
				FVoxelViewKey Neighbor = Node;
				Neighbor.Coordinate[Axis] += Sign;
				while (Neighbor.Level <= RootLevel && !Leaves.Contains(Neighbor))
				{
					Neighbor = Neighbor.GetParent();
				}
				if (Neighbor.Level <= Node.Level + 1 || Neighbor.Level > RootLevel)
				{
					continue;
				}
				Split(Neighbor);
				TArray<FVoxelViewKey> Children;
				Neighbor.GetChildren(Children);
				BalanceQueue.Append(Children);
				BalanceQueue.Add(Node);
			}
		}
	}
	FineDependencies.Reset();
	for (const FVoxelViewKey& Node : Leaves)
	{
		if (Node.Level == 0)
		{
			FineDependencies.Add(Node);
		}
	}
	OverBudgetLeaves = FMath::Max(0, Leaves.Num() - InMaximumLeaves);
}

bool FVoxelTerrainViewPlan::ResolveNode(const FVoxelViewKey& InNode,
	TFunctionRef<bool(const FVoxelViewKey&)> InIsReady, const TSet<FVoxelViewKey>& InPreviousAncestors,
	const TSet<FVoxelViewKey>* InPrevious, const TSet<FVoxelViewKey>* InAvailableBranches,
	TArray<FVoxelViewKey>& OutVisible) const
{
	if (InAvailableBranches && !InAvailableBranches->Contains(InNode))
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
			bChildrenReady &= ResolveNode(Child, InIsReady, InPreviousAncestors, InPrevious, InAvailableBranches, OutVisible);
		}
		if (bChildrenReady)
		{
			return true;
		}
	}
	// 已呈现父节点需要完整交接；初次加载的空区域直接逐叶发布，不构建粗占位网格。
	if (InPrevious && InPrevious->Contains(InNode) && InIsReady(InNode))
	{
		OutVisible.SetNum(FirstChild, EAllowShrinking::No);
		OutVisible.Add(InNode);
		return true;
	}
	return false;
}

void FVoxelTerrainViewPlan::ResolveVisible(TFunctionRef<bool(const FVoxelViewKey&)> InIsReady,
	TSet<FVoxelViewKey>& OutVisible, const TSet<FVoxelViewKey>* InPrevious,
	const TSet<FVoxelViewKey>* InReadyNodes) const
{
	TSet<FVoxelViewKey> PreviousAncestors;
	if (InPrevious)
	{
		for (FVoxelViewKey Node : *InPrevious)
		{
			while (!Roots.Contains(Node) && Node.Level < 24)
			{
				Node = Node.GetParent();
				bool bAlreadyPresent = false;
				PreviousAncestors.Add(Node, &bAlreadyPresent);
				if (bAlreadyPresent) break;
			}
		}
	}
	TSet<FVoxelViewKey> AvailableBranches;
	if (InReadyNodes)
	{
		AvailableBranches = PreviousAncestors;
		if (InPrevious) AvailableBranches.Append(*InPrevious);
		for (FVoxelViewKey Node : *InReadyNodes)
		{
			while (Node.Level < 24)
			{
				bool bAlreadyPresent = false;
				AvailableBranches.Add(Node, &bAlreadyPresent);
				if (bAlreadyPresent || Roots.Contains(Node)) break;
				Node = Node.GetParent();
			}
		}
	}
	// 上一已提交分区包含合法空节点；没有Actor不等于失去已发布的空间所有权。
	auto IsAvailable = [&InIsReady, InPrevious](const FVoxelViewKey& Key)
	{
		return InIsReady(Key) || (InPrevious && InPrevious->Contains(Key));
	};
	OutVisible.Reset();
	for (const FVoxelViewKey& Root : Roots)
	{
		TArray<FVoxelViewKey> Visible;
		ResolveNode(Root, IsAvailable, PreviousAncestors, InPrevious, InReadyNodes ? &AvailableBranches : nullptr, Visible);
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
