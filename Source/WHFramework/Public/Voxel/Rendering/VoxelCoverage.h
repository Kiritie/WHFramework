#pragma once

#include "CoreMinimal.h"

struct FVoxelVoxelProxyData;

struct WHFRAMEWORK_API FVoxelCoverageRect
{
	FIntPoint Min = FIntPoint::ZeroValue;
	FIntPoint Max = FIntPoint::ZeroValue;

	bool IsValid() const { return Min.X < Max.X && Min.Y < Max.Y; }
	bool ContainsCell(const FIntPoint& InPoint) const
	{
		return InPoint.X >= Min.X && InPoint.Y >= Min.Y && InPoint.X < Max.X && InPoint.Y < Max.Y;
	}
	bool Contains(const FVoxelCoverageRect& InOther) const
	{
		return InOther.IsValid() && InOther.Min.X >= Min.X && InOther.Min.Y >= Min.Y &&
			InOther.Max.X <= Max.X && InOther.Max.Y <= Max.Y;
	}
	bool Intersects(const FVoxelCoverageRect& InOther) const
	{
		return Min.X < InOther.Max.X && Max.X > InOther.Min.X && Min.Y < InOther.Max.Y && Max.Y > InOther.Min.Y;
	}
};

struct WHFRAMEWORK_API FVoxelCoverageBox
{
	FIntVector Min = FIntVector::ZeroValue;
	FIntVector Max = FIntVector::ZeroValue;

	bool IsValid() const { return Min.X < Max.X && Min.Y < Max.Y && Min.Z < Max.Z; }
	bool ContainsCell(const FIntVector& InPoint) const
	{
		return InPoint.X >= Min.X && InPoint.Y >= Min.Y && InPoint.Z >= Min.Z &&
			InPoint.X < Max.X && InPoint.Y < Max.Y && InPoint.Z < Max.Z;
	}
	bool Contains(const FVoxelCoverageBox& InOther) const
	{
		return InOther.IsValid() && InOther.Min.X >= Min.X && InOther.Min.Y >= Min.Y && InOther.Min.Z >= Min.Z &&
			InOther.Max.X <= Max.X && InOther.Max.Y <= Max.Y && InOther.Max.Z <= Max.Z;
	}
	bool Intersects(const FVoxelCoverageBox& InOther) const
	{
		return Min.X < InOther.Max.X && Max.X > InOther.Min.X && Min.Y < InOther.Max.Y && Max.Y > InOther.Min.Y &&
			Min.Z < InOther.Max.Z && Max.Z > InOther.Min.Z;
	}
	FVoxelCoverageRect ProjectXY() const { return { FIntPoint(Min.X, Min.Y), FIntPoint(Max.X, Max.Y) }; }
};

namespace VoxelCoverage
{
	WHFRAMEWORK_API bool IsFullyCovered2D(const FVoxelCoverageRect& InTarget, TConstArrayView<FVoxelCoverageRect> InCoverage);
	WHFRAMEWORK_API bool IsFullyCovered3D(const FVoxelCoverageBox& InTarget, TConstArrayView<FVoxelCoverageBox> InCoverage);
	WHFRAMEWORK_API bool IntersectsAny2D(const FVoxelCoverageRect& InTarget, TConstArrayView<FVoxelCoverageRect> InCoverage);
	WHFRAMEWORK_API bool IntersectsAny3D(const FVoxelCoverageBox& InTarget, TConstArrayView<FVoxelCoverageBox> InCoverage);
	WHFRAMEWORK_API bool HasProxyTopSurfaceInColumn(const FVoxelVoxelProxyData& InData, int32 InX, int32 InY);
}
