#include "Voxel/Rendering/VoxelPublishGroups.h"

#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"

namespace
{
	bool IsTransitionOwner(
		const FVoxelPublishFootprint& InTransition,
		const FVoxelPublishFootprint& InOwner)
	{
		if (!InTransition.bTransition || InOwner.bTransition ||
			InTransition.Key.Coordinate != InOwner.Key.Coordinate ||
			InTransition.Key.Level != InOwner.Key.Level)
		{
			return false;
		}
		return (InTransition.Key.Representation == 4 && InOwner.Key.Representation == 2) ||
			(InTransition.Key.Representation == 5 && InOwner.Key.Representation == 3);
	}

	bool Interacts(
		const FVoxelPublishFootprint& A,
		const FVoxelPublishFootprint& B)
	{
		if (!A.Bounds.IsValid || !B.Bounds.IsValid)
		{
			return false;
		}

		const double X =
			FMath::Min(A.Bounds.Max.X, B.Bounds.Max.X) -
			FMath::Max(A.Bounds.Min.X, B.Bounds.Min.X);
		const double Y =
			FMath::Min(A.Bounds.Max.Y, B.Bounds.Max.Y) -
			FMath::Max(A.Bounds.Min.Y, B.Bounds.Min.Y);

		if (A.bTransition || B.bTransition)
		{
			return (X >= 0.0 && Y > 0.0) ||
				(Y >= 0.0 && X > 0.0);
		}

		const double Z = FMath::Min(A.Bounds.Max.Z, B.Bounds.Max.Z) - FMath::Max(A.Bounds.Min.Z, B.Bounds.Min.Z);
		return X > 0.0 && Y > 0.0 && (A.Key.Representation >= 2 || B.Key.Representation >= 2 || Z > 0.0);
	}

	int32 FindRoot(TArray<int32>& InParents, int32 InIndex)
	{
		while (InParents[InIndex] != InIndex)
		{
			InParents[InIndex] = InParents[InParents[InIndex]];
			InIndex = InParents[InIndex];
		}
		return InIndex;
	}
}

int32 FVoxelPublishGroupContinuation::FindRoot(int32 InIndex)
{
	while (Parents[InIndex] != InIndex)
	{
		Parents[InIndex] = Parents[Parents[InIndex]];
		InIndex = Parents[InIndex];
	}
	return InIndex;
}

void FVoxelPublishGroupContinuation::RefreshInterval(const int32 InNode)
{
	auto& Node = Intervals[InNode];
	Node.MaximumEnd = Footprints[Node.Footprint].Bounds.Max.X;
	Node.MinimumY = Footprints[Node.Footprint].Bounds.Min.Y;
	Node.MaximumY = Footprints[Node.Footprint].Bounds.Max.Y;
	for (const int32 Child : {Node.Left, Node.Right})
	{
		if (Child == INDEX_NONE) continue;
		Node.MaximumEnd = FMath::Max(Node.MaximumEnd, Intervals[Child].MaximumEnd);
		Node.MinimumY = FMath::Min(Node.MinimumY, Intervals[Child].MinimumY);
		Node.MaximumY = FMath::Max(Node.MaximumY, Intervals[Child].MaximumY);
	}
}

int32 FVoxelPublishGroupContinuation::InsertInterval(const int32 InRoot, const int32 InNode)
{
	if (InRoot == INDEX_NONE) return InNode;
	const double NodeMin = Footprints[Intervals[InNode].Footprint].Bounds.Min.X;
	const double RootMin = Footprints[Intervals[InRoot].Footprint].Bounds.Min.X;
	if (NodeMin < RootMin || (NodeMin == RootMin && InNode < InRoot))
	{
		const int32 Child = InsertInterval(Intervals[InRoot].Left, InNode);
		Intervals[InRoot].Left = Child;
		if (Intervals[Child].Priority < Intervals[InRoot].Priority)
		{
			Intervals[InRoot].Left = Intervals[Child].Right;
			Intervals[Child].Right = InRoot;
			RefreshInterval(InRoot);
			RefreshInterval(Child);
			return Child;
		}
	}
	else
	{
		const int32 Child = InsertInterval(Intervals[InRoot].Right, InNode);
		Intervals[InRoot].Right = Child;
		if (Intervals[Child].Priority < Intervals[InRoot].Priority)
		{
			Intervals[InRoot].Right = Intervals[Child].Left;
			Intervals[Child].Left = InRoot;
			RefreshInterval(InRoot);
			RefreshInterval(Child);
			return Child;
		}
	}
	RefreshInterval(InRoot);
	return InRoot;
}

bool FVoxelPublishGroupContinuation::Add(const FVoxelPublishFootprint& InFootprint)
{
	if (Phase != 0 || Footprints.Num() >= 8192) return false;
	const int32 Index = Footprints.Add(InFootprint);
	Parents.Add(Index);
	GroupIndices.Add(INDEX_NONE);
	if (const int32* Previous = Keys.Find(InFootprint.Key)) Parents[FindRoot(*Previous)] = Index;
	else Keys.Add(InFootprint.Key, Index);
	if (!InFootprint.bTransition) Owners.Add(InFootprint.Key, Index);
	if (InFootprint.Bounds.IsValid)
	{
		FInterval Node;
		Node.Footprint = Index;
		uint32 Priority = static_cast<uint32>(Index + 1);
		Priority ^= Priority >> 16;
		Priority *= 0x7feb352du;
		Priority ^= Priority >> 15;
		Priority *= 0x846ca68bu;
		Node.Priority = Priority ^ (Priority >> 16);
		Node.MaximumEnd = InFootprint.Bounds.Max.X;
		Node.MinimumY = InFootprint.Bounds.Min.Y;
		Node.MaximumY = InFootprint.Bounds.Max.Y;
		const int32 NodeIndex = Intervals.Add(Node);
		IntervalRoot = InsertInterval(IntervalRoot, NodeIndex);
	}
	return true;
}

bool FVoxelPublishGroupContinuation::Advance(const int32 InMaximumOperations, const double InDeadlineSeconds)
{
	if (Phase == 0) { Phase = 1; Cursor = 0; }
	for (int32 Operation = 0; Operation < FMath::Max(0, InMaximumOperations) &&
		FPlatformTime::Seconds() < InDeadlineSeconds; ++Operation)
	{
		if (Phase == 1)
		{
			if (!Footprints.IsValidIndex(Cursor)) { Phase = 2; Cursor = 0; continue; }
			const auto& Transition = Footprints[Cursor];
			if (Transition.bTransition)
			{
				auto Owner = Transition.Key;
				if (Owner.Representation == 4) Owner.Representation = 2;
				else if (Owner.Representation == 5) Owner.Representation = 3;
				if (const int32* Index = Owners.Find(Owner); Index && IsTransitionOwner(Transition, Footprints[*Index]))
					Parents[FindRoot(*Index)] = FindRoot(Cursor);
			}
			++Cursor;
		}
		else if (Phase == 2)
		{
			if (!Footprints.IsValidIndex(Cursor)) { Phase = 3; Cursor = 0; continue; }
			const auto& Current = Footprints[Cursor];
			if (!Current.Bounds.IsValid) { ++Cursor; bQueryStarted = false; continue; }
			if (!bQueryStarted)
			{
				if (IntervalRoot != INDEX_NONE) QueryStack.Add(IntervalRoot);
				bQueryStarted = true;
			}
			if (QueryStack.IsEmpty()) { ++Cursor; bQueryStarted = false; continue; }
			const auto& Node = Intervals[QueryStack.Pop(EAllowShrinking::No)];
			if (Node.MaximumEnd < Current.Bounds.Min.X || Node.MaximumY < Current.Bounds.Min.Y ||
				Node.MinimumY > Current.Bounds.Max.Y) continue;
			const auto& Other = Footprints[Node.Footprint];
			if (Node.Left != INDEX_NONE) QueryStack.Add(Node.Left);
			if (Other.Bounds.Min.X > Current.Bounds.Max.X) continue;
			if (Node.Right != INDEX_NONE) QueryStack.Add(Node.Right);
			if (Node.Footprint >= Cursor) continue;
			++ComparedFootprints;
			if (Interacts(Current, Other)) Parents[FindRoot(Node.Footprint)] = FindRoot(Cursor);
		}
		else if (Phase == 3)
		{
			if (!Footprints.IsValidIndex(Cursor)) { Phase = 4; return true; }
			const int32 Root = FindRoot(Cursor);
			GroupIndices[Cursor++] = Ordinals.FindOrAdd(Root, Ordinals.Num());
		}
		else return true;
	}
	return Phase == 4;
}

const TArray<int32>& FVoxelPublishGroupContinuation::GetGroupIndices() const
{
	check(Phase == 4);
	return GroupIndices;
}

uint64 FVoxelPublishGroupContinuation::GetComparedFootprints() const
{
	return ComparedFootprints;
}

FBox FVoxelPublishGroupPlanner::OwnershipBounds(
	const FVoxelPublishGroupKey& InKey,
	const double InBlockSize, const FVoxelLodTopologySettings& InLod)
{
	if (!FMath::IsFinite(InBlockSize) || InBlockSize <= 0.0 || InKey.Level > 20)
	{
		return FBox(ForceInit);
	}

	int64 BaseSide = 0;
	switch (InKey.Representation)
	{
	case 0:
	case 1:
		BaseSide = FVoxelViewKey().GetSide();
		break;
	case 2:
	case 4:
		BaseSide = 16;
		break;
	case 3:
	case 5:
		BaseSide = 16;
		break;
	default:
		return FBox(ForceInit);
	}
	const int64 Side = BaseSide << InKey.Level;
	const double X0 = static_cast<double>(InKey.Coordinate.X) * Side * InBlockSize;
	const double Y0 = static_cast<double>(InKey.Coordinate.Y) * Side * InBlockSize;
	const double X1 = X0 + static_cast<double>(Side) * InBlockSize;
	const double Y1 = Y0 + static_cast<double>(Side) * InBlockSize;
	const double Z0 = InKey.Representation < 2 ? static_cast<double>(InKey.Coordinate.Z) * Side * InBlockSize : 0.0;
	const double Z1 = InKey.Representation < 2 ? Z0 + Side * InBlockSize : 1.0;
	return FBox(FVector(X0, Y0, Z0), FVector(X1, Y1, Z1));
}

FBox FVoxelPublishGroupPlanner::PublicationBounds(
	const FVoxelPublishGroupKey& InKey,
	const double InBlockSize,
	const FBox& InPresentedBounds,
	const FBox& InDesiredBounds,
	const bool bInTransition, const FVoxelLodTopologySettings& InLod)
{
	FBox Bounds = bInTransition ? FBox(ForceInit) : OwnershipBounds(InKey, InBlockSize, InLod);
	Bounds += InPresentedBounds;
	Bounds += InDesiredBounds;
	return Bounds;
}

void FVoxelPublishGroupPlanner::Build(
	TConstArrayView<FVoxelPublishFootprint> InFootprints,
	TArray<int32>& OutGroupIndices)
{
	const int32 Count = InFootprints.Num();
	OutGroupIndices.SetNumUninitialized(Count);
	TArray<int32> Parents;
	Parents.SetNumUninitialized(Count);
	TArray<int32> Order;
	Order.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Parents[Index] = Index;
		if (InFootprints[Index].Bounds.IsValid)
		{
			Order.Add(Index);
		}
	}
	Order.Sort([&](const int32 A, const int32 B)
	{
		const double AX = InFootprints[A].Bounds.Min.X;
		const double BX = InFootprints[B].Bounds.Min.X;
		return AX == BX ? A < B : AX < BX;
	});

	TArray<int32> Active;
	for (const int32 Index : Order)
	{
		const FVoxelPublishFootprint& Current = InFootprints[Index];
		for (int32 ActiveIndex = Active.Num() - 1; ActiveIndex >= 0; --ActiveIndex)
		{
			const int32 OtherIndex = Active[ActiveIndex];
			if (InFootprints[OtherIndex].Bounds.Max.X < Current.Bounds.Min.X)
			{
				Active.RemoveAtSwap(ActiveIndex, 1, EAllowShrinking::No);
				continue;
			}
			if (Interacts(Current, InFootprints[OtherIndex]))
			{
				Parents[FindRoot(Parents, OtherIndex)] = FindRoot(Parents, Index);
			}
		}
		Active.Add(Index);
	}
	TMap<FVoxelPublishGroupKey, int32> Owners;
	TMap<FVoxelPublishGroupKey, int32> Keys;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// 同一 owner 的实体、水面和不同 Z section 必须使用同一次提交。
		if (const int32* Other = Keys.Find(InFootprints[Index].Key))
			Parents[FindRoot(Parents, *Other)] = FindRoot(Parents, Index);
		else Keys.Add(InFootprints[Index].Key, Index);
		if (!InFootprints[Index].bTransition)
		{
			Owners.Add(InFootprints[Index].Key, Index);
		}
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVoxelPublishFootprint& Transition = InFootprints[Index];
		if (!Transition.bTransition) continue;
		FVoxelPublishGroupKey OwnerKey = Transition.Key;
		if (OwnerKey.Representation == 4) OwnerKey.Representation = 2;
		else if (OwnerKey.Representation == 5) OwnerKey.Representation = 3;
		else continue;
		if (const int32* OwnerIndex = Owners.Find(OwnerKey);
			OwnerIndex && IsTransitionOwner(Transition, InFootprints[*OwnerIndex]))
		{
			Parents[FindRoot(Parents, *OwnerIndex)] = FindRoot(Parents, Index);
		}
	}

	TMap<int32, int32> Ordinals;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 Root = FindRoot(Parents, Index);
		OutGroupIndices[Index] = Ordinals.FindOrAdd(Root, Ordinals.Num());
	}
}

void FVoxelPublishBoundsIndex::Add(const int32 InOrdinal, const FBox& InBounds)
{
	if (!InBounds.IsValid) return;
	Bounds.Add(InOrdinal, InBounds);
	uint8 Level = 0;
	double Side = 800.0;
	const double Extent = FMath::Max(InBounds.Max.X - InBounds.Min.X, InBounds.Max.Y - InBounds.Min.Y);
	while (Side < Extent && Level < 31) { ++Level; Side *= 2.0; }
	const FIntPoint Min(FMath::FloorToInt(InBounds.Min.X / Side), FMath::FloorToInt(InBounds.Min.Y / Side));
	const FIntPoint Max(FMath::FloorToInt(InBounds.Max.X / Side), FMath::FloorToInt(InBounds.Max.Y / Side));
	for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
		for (int32 X = Min.X; X <= Max.X; ++X)
		{
			FKey Key{FIntPoint(X, Y), Level};
			Nodes.FindOrAdd(Key).Own.Add(InOrdinal);
			while (Key.Level < 31)
			{
				Key.Coordinate = FIntPoint(static_cast<int32>(VoxelStreamingGeometry::FloorDivide(Key.Coordinate.X, 2)),
					static_cast<int32>(VoxelStreamingGeometry::FloorDivide(Key.Coordinate.Y, 2)));
				++Key.Level;
				Nodes.FindOrAdd(Key);
			}
		}
}

void FVoxelPublishBoundsIndex::BeginQuery(const FBox& InBounds, FQuery& OutQuery) const
{
	OutQuery = {};
	OutQuery.Bounds = InBounds;
	if (!InBounds.IsValid) return;
	for (int32 Y : {-1, 0}) for (int32 X : {-1, 0}) OutQuery.Stack.Add({FIntPoint(X, Y), 31});
}

bool FVoxelPublishBoundsIndex::AdvanceQuery(FQuery& InOutQuery, int32& OutOrdinal) const
{
	OutOrdinal = INDEX_NONE;
	if (InOutQuery.bHasCurrent)
	{
		const auto& Own = Nodes.FindChecked(InOutQuery.Current).Own;
		if (InOutQuery.OwnCursor < Own.GetMaxIndex())
		{
			const auto Id = FSetElementId::FromInteger(InOutQuery.OwnCursor++);
			if (!Own.IsValidId(Id)) return false;
			const int32 Candidate = Own[Id];
			if (InOutQuery.Seen.Contains(Candidate)) return false;
			InOutQuery.Seen.Add(Candidate);
			const FBox& Box = Bounds.FindChecked(Candidate);
			if (Box.Min.X <= InOutQuery.Bounds.Max.X && Box.Max.X >= InOutQuery.Bounds.Min.X &&
				Box.Min.Y <= InOutQuery.Bounds.Max.Y && Box.Max.Y >= InOutQuery.Bounds.Min.Y) OutOrdinal = Candidate;
			return false;
		}
		InOutQuery.bHasCurrent = false;
	}
	if (InOutQuery.Stack.IsEmpty()) return true;
	const FKey Key = InOutQuery.Stack.Pop(EAllowShrinking::No);
	const FNode* Node = Nodes.Find(Key);
	if (!Node) return false;
	const double Side = 800.0 * static_cast<double>(1ull << Key.Level);
	const double MinX = Key.Coordinate.X * Side, MinY = Key.Coordinate.Y * Side;
	if (MinX > InOutQuery.Bounds.Max.X || MinX + Side < InOutQuery.Bounds.Min.X ||
		MinY > InOutQuery.Bounds.Max.Y || MinY + Side < InOutQuery.Bounds.Min.Y) return false;
	InOutQuery.Current = Key;
	InOutQuery.OwnCursor = 0;
	InOutQuery.bHasCurrent = !Node->Own.IsEmpty();
	if (Key.Level > 0)
		for (int32 Child = 0; Child < 4; ++Child)
			InOutQuery.Stack.Add({Key.Coordinate * 2 + FIntPoint(Child & 1, (Child >> 1) & 1), uint8(Key.Level - 1)});
	return false;
}
