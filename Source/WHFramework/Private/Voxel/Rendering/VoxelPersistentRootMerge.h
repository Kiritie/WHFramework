#pragma once

#include "CoreMinimal.h"

enum class EVoxelPersistentMergeProgress : uint8
{
	Working,
	Complete,
	CapacityBlocked,
	Conflict
};

// 不变子树直接共享。只展开两份增量同时修改的路径，每个步骤至多访问一个节点。
template<typename NodeType, int32 ChildCount, int32 MaximumDepth>
class TVoxelPersistentRootMerge
{
public:
	using FNode = TSharedPtr<const NodeType, ESPMode::ThreadSafe>;
	using FMutableNode = TSharedPtr<NodeType, ESPMode::ThreadSafe>;

	void Begin(const FNode& InBase, const FNode& InCurrent, const FNode& InPrepared)
	{
		check(Walk.IsEmpty());
		Result.Reset();
		Walk.Add({InBase, InCurrent, InPrepared});
		bComplete = false;
	}

	void Reset()
	{
		Walk.Reset();
		Result.Reset();
		bComplete = false;
	}

	const FNode& GetResult() const
	{
		check(bComplete);
		return Result;
	}

	template<typename SameType, typename LeafType, typename MergeLeafType, typename AllocateType, typename FinishType>
	EVoxelPersistentMergeProgress Step(SameType&& Same, LeafType&& IsLeaf, MergeLeafType&& MergeLeaf,
		AllocateType&& Allocate, FinishType&& Finish)
	{
		if (bComplete) return EVoxelPersistentMergeProgress::Complete;
		check(!Walk.IsEmpty());
		auto& Frame = Walk.Last();
		if (Same(Frame.Prepared, Frame.Base)) return CompleteNode(Frame.Current);
		if (Same(Frame.Current, Frame.Base) || Same(Frame.Current, Frame.Prepared)) return CompleteNode(Frame.Prepared);
		if (IsLeaf(Frame.Base, Frame.Current, Frame.Prepared))
		{
			FNode Leaf;
			const auto Progress = MergeLeaf(Frame.Base, Frame.Current, Frame.Prepared, Leaf);
			if (Progress != EVoxelPersistentMergeProgress::Complete) return Progress;
			return CompleteNode(MoveTemp(Leaf));
		}
		if (!Frame.Result)
		{
			if (!Allocate(Frame.Base, Frame.Current, Frame.Prepared, Frame.Result))
				return EVoxelPersistentMergeProgress::CapacityBlocked;
			return EVoxelPersistentMergeProgress::Working;
		}
		if (Frame.Child < ChildCount)
		{
			const int32 Child = Frame.Child++;
			check(Walk.Num() < MaximumDepth);
			Walk.Add({Frame.Base ? Frame.Base->Children[Child] : nullptr,
				Frame.Current ? Frame.Current->Children[Child] : nullptr,
				Frame.Prepared ? Frame.Prepared->Children[Child] : nullptr});
			return EVoxelPersistentMergeProgress::Working;
		}
		Finish(*Frame.Result);
		return CompleteNode(Frame.Result);
	}

private:
	struct FWalk
	{
		FNode Base;
		FNode Current;
		FNode Prepared;
		FMutableNode Result;
		int32 Child = 0;
	};

	EVoxelPersistentMergeProgress CompleteNode(FNode InNode)
	{
		Walk.Pop(EAllowShrinking::No);
		if (Walk.IsEmpty())
		{
			Result = MoveTemp(InNode);
			bComplete = true;
			return EVoxelPersistentMergeProgress::Complete;
		}
		auto& Parent = Walk.Last();
		Parent.Result->Children[Parent.Child - 1] = MoveTemp(InNode);
		return EVoxelPersistentMergeProgress::Working;
	}

	TArray<FWalk, TInlineAllocator<MaximumDepth>> Walk;
	FNode Result;
	bool bComplete = false;
};
