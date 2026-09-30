#include "Voxel/Rendering/VoxelPublishGroups.h"

#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Rendering/VoxelViewTypes.h"

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

		return X > 0.0 && Y > 0.0;
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

FBox FVoxelPublishGroupPlanner::OwnershipBounds(
	const FVoxelPublishGroupKey& InKey,
	const double InBlockSize)
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
		BaseSide = FVoxelSurfaceTileData::CellSide;
		break;
	case 3:
	case 5:
		BaseSide = static_cast<int64>(FVoxelMacroTileData::CellSide) *
			FVoxelMacroTileData::BaseStep;
		break;
	default:
		return FBox(ForceInit);
	}
	const int64 Side = BaseSide << InKey.Level;
	const double X0 = static_cast<double>(InKey.Coordinate.X) * Side * InBlockSize;
	const double Y0 = static_cast<double>(InKey.Coordinate.Y) * Side * InBlockSize;
	const double X1 = X0 + static_cast<double>(Side) * InBlockSize;
	const double Y1 = Y0 + static_cast<double>(Side) * InBlockSize;
	return FBox(FVector(X0, Y0, 0.0), FVector(X1, Y1, 1.0));
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
	for (int32 Index = 0; Index < Count; ++Index)
	{
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
