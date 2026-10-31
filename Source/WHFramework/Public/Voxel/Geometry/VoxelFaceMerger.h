#pragma once

#include "CoreMinimal.h"

class FVoxelFaceMerger
{
public:
	template<typename Matches>
	static bool Grow(const FIntPoint& InMin, const FIntPoint& InLimit, Matches&& InMatches, FIntRect& OutRect, const TAtomic<bool>* InCancel = nullptr)
	{
		if (InCancel && InCancel->Load()) return false;
		int32 EndX = InMin.X + 1;
		while (EndX < InLimit.X && InMatches(EndX, InMin.Y))
		{
			if ((EndX & 63) == 0 && InCancel && InCancel->Load()) return false;
			++EndX;
		}
		int32 EndY = InMin.Y + 1;
		while (EndY < InLimit.Y)
		{
			if (InCancel && InCancel->Load()) return false;
			bool bMatchesRow = true;
			for (int32 X = InMin.X; X < EndX; ++X)
			{
				if (!InMatches(X, EndY))
				{
					bMatchesRow = false;
					break;
				}
			}
			if (!bMatchesRow) break;
			++EndY;
		}
		OutRect = FIntRect(InMin, FIntPoint(EndX, EndY));
		return true;
	}

	static bool Join(const FBox2D& InFirst, const FBox2D& InSecond, FBox2D& OutRect)
	{
		if ((InFirst.Min.X == InSecond.Min.X && InFirst.Max.X == InSecond.Max.X &&
			(InFirst.Max.Y == InSecond.Min.Y || InSecond.Max.Y == InFirst.Min.Y)) ||
			(InFirst.Min.Y == InSecond.Min.Y && InFirst.Max.Y == InSecond.Max.Y &&
			(InFirst.Max.X == InSecond.Min.X || InSecond.Max.X == InFirst.Min.X)))
		{
			OutRect = FBox2D(FVector2D(FMath::Min(InFirst.Min.X, InSecond.Min.X), FMath::Min(InFirst.Min.Y, InSecond.Min.Y)),
				FVector2D(FMath::Max(InFirst.Max.X, InSecond.Max.X), FMath::Max(InFirst.Max.Y, InSecond.Max.Y)));
			return true;
		}
		return false;
	}
};

template<typename KeyType, int32 Capacity = 64>
class TVoxelPlanarFaceMerger
{
	static_assert(Capacity > 0);

public:
	template<typename Emit>
	bool Add(const uint8 InFace, const double InPlane, FBox2D InRect, const KeyType& InKey, const bool bInOccludes, Emit&& InEmit)
	{
		for (int32 Index = 0; Index < Count;)
		{
			const FEntry& Entry = Entries[Index];
			if (Entry.Plane != InPlane || Entry.Face / 2 != InFace / 2)
			{
				++Index;
				continue;
			}
			if (Entry.Face == (InFace ^ 1) && Entry.bOccludes && bInOccludes && Entry.Rect == InRect)
			{
				--Count;
				if (Index != Count) Entries[Index] = MoveTemp(Entries[Count]);
				return true;
			}
			FBox2D Joined;
			if (Entry.Face == InFace && Entry.Key == InKey && Entry.bOccludes == bInOccludes && FVoxelFaceMerger::Join(Entry.Rect, InRect, Joined))
			{
				InRect = Joined;
				--Count;
				if (Index != Count) Entries[Index] = MoveTemp(Entries[Count]);
				Index = 0;
				continue;
			}
			++Index;
		}
		if (Count == Capacity)
		{
			if (!InEmit(Entries[0].Face, Entries[0].Plane, Entries[0].Rect, Entries[0].Key)) return false;
			--Count;
			if (Count) Entries[0] = MoveTemp(Entries[Count]);
		}
		Entries[Count++] = {InRect, InKey, InPlane, InFace, bInOccludes};
		return true;
	}

	template<typename Emit>
	bool Flush(Emit&& InEmit)
	{
		for (int32 Index = 0; Index < Count; ++Index)
			if (!InEmit(Entries[Index].Face, Entries[Index].Plane, Entries[Index].Rect, Entries[Index].Key)) return false;
		Count = 0;
		return true;
	}

private:
	struct FEntry
	{
		FBox2D Rect;
		KeyType Key;
		double Plane = 0.0;
		uint8 Face = 0;
		bool bOccludes = false;
	};
	FEntry Entries[Capacity];
	int32 Count = 0;
};
