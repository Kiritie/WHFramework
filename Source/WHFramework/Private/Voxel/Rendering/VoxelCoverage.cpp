#include "Voxel/Rendering/VoxelCoverage.h"

#include "Voxel/Rendering/VoxelProxyBuilder.h"

namespace
{
	constexpr int32 MaximumCoverageEdgesPerAxis = 64;

	void SortUnique(TArray<int32>& InOutValues)
	{
		InOutValues.Sort();
		for (int32 Index = InOutValues.Num() - 1; Index > 0; --Index)
		{
			if (InOutValues[Index] == InOutValues[Index - 1]) InOutValues.RemoveAt(Index, 1, EAllowShrinking::No);
		}
	}
}

bool VoxelCoverage::IsFullyCovered2D(const FVoxelCoverageRect& InTarget, const TConstArrayView<FVoxelCoverageRect> InCoverage)
{
	if (!InTarget.IsValid()) return false;
	TArray<FVoxelCoverageRect> Clipped;
	TArray<int32> XEdges { InTarget.Min.X, InTarget.Max.X };
	TArray<int32> YEdges { InTarget.Min.Y, InTarget.Max.Y };
	for (const FVoxelCoverageRect& Coverage : InCoverage)
	{
		if (!Coverage.IsValid() || !Coverage.Intersects(InTarget)) continue;
		const FVoxelCoverageRect Rect {
			FIntPoint(FMath::Max(Coverage.Min.X, InTarget.Min.X), FMath::Max(Coverage.Min.Y, InTarget.Min.Y)),
			FIntPoint(FMath::Min(Coverage.Max.X, InTarget.Max.X), FMath::Min(Coverage.Max.Y, InTarget.Max.Y))
		};
		if (!Rect.IsValid()) continue;
		Clipped.Add(Rect);
		XEdges.Append({ Rect.Min.X, Rect.Max.X });
		YEdges.Append({ Rect.Min.Y, Rect.Max.Y });
	}
	if (Clipped.IsEmpty()) return false;
	SortUnique(XEdges); SortUnique(YEdges);
	if (XEdges.Num() > MaximumCoverageEdgesPerAxis || YEdges.Num() > MaximumCoverageEdgesPerAxis) return false;
	for (int32 X = 0; X + 1 < XEdges.Num(); ++X)
	{
		for (int32 Y = 0; Y + 1 < YEdges.Num(); ++Y)
		{
			if (XEdges[X] >= XEdges[X + 1] || YEdges[Y] >= YEdges[Y + 1]) continue;
			const FIntPoint Probe(XEdges[X], YEdges[Y]);
			if (!Clipped.ContainsByPredicate([&Probe](const FVoxelCoverageRect& Rect) { return Rect.ContainsCell(Probe); })) return false;
		}
	}
	return true;
}

bool VoxelCoverage::IsFullyCovered3D(const FVoxelCoverageBox& InTarget, const TConstArrayView<FVoxelCoverageBox> InCoverage)
{
	if (!InTarget.IsValid()) return false;
	TArray<FVoxelCoverageBox> Clipped;
	TArray<int32> XEdges { InTarget.Min.X, InTarget.Max.X };
	TArray<int32> YEdges { InTarget.Min.Y, InTarget.Max.Y };
	TArray<int32> ZEdges { InTarget.Min.Z, InTarget.Max.Z };
	for (const FVoxelCoverageBox& Coverage : InCoverage)
	{
		if (!Coverage.IsValid() || !Coverage.Intersects(InTarget)) continue;
		const FVoxelCoverageBox Box {
			FIntVector(FMath::Max(Coverage.Min.X, InTarget.Min.X), FMath::Max(Coverage.Min.Y, InTarget.Min.Y), FMath::Max(Coverage.Min.Z, InTarget.Min.Z)),
			FIntVector(FMath::Min(Coverage.Max.X, InTarget.Max.X), FMath::Min(Coverage.Max.Y, InTarget.Max.Y), FMath::Min(Coverage.Max.Z, InTarget.Max.Z))
		};
		if (!Box.IsValid()) continue;
		Clipped.Add(Box);
		XEdges.Append({ Box.Min.X, Box.Max.X }); YEdges.Append({ Box.Min.Y, Box.Max.Y }); ZEdges.Append({ Box.Min.Z, Box.Max.Z });
	}
	if (Clipped.IsEmpty()) return false;
	SortUnique(XEdges); SortUnique(YEdges); SortUnique(ZEdges);
	if (XEdges.Num() > MaximumCoverageEdgesPerAxis || YEdges.Num() > MaximumCoverageEdgesPerAxis || ZEdges.Num() > MaximumCoverageEdgesPerAxis) return false;
	for (int32 X = 0; X + 1 < XEdges.Num(); ++X)
	{
		for (int32 Y = 0; Y + 1 < YEdges.Num(); ++Y)
		{
			for (int32 Z = 0; Z + 1 < ZEdges.Num(); ++Z)
			{
				if (XEdges[X] >= XEdges[X + 1] || YEdges[Y] >= YEdges[Y + 1] || ZEdges[Z] >= ZEdges[Z + 1]) continue;
				const FIntVector Probe(XEdges[X], YEdges[Y], ZEdges[Z]);
				if (!Clipped.ContainsByPredicate([&Probe](const FVoxelCoverageBox& Box) { return Box.ContainsCell(Probe); })) return false;
			}
		}
	}
	return true;
}

bool VoxelCoverage::IntersectsAny2D(const FVoxelCoverageRect& InTarget, const TConstArrayView<FVoxelCoverageRect> InCoverage)
{
	return InTarget.IsValid() && InCoverage.ContainsByPredicate([&InTarget](const FVoxelCoverageRect& Coverage)
	{
		return Coverage.IsValid() && Coverage.Intersects(InTarget);
	});
}

bool VoxelCoverage::IntersectsAny3D(const FVoxelCoverageBox& InTarget, const TConstArrayView<FVoxelCoverageBox> InCoverage)
{
	return InTarget.IsValid() && InCoverage.ContainsByPredicate([&InTarget](const FVoxelCoverageBox& Coverage)
	{
		return Coverage.IsValid() && Coverage.Intersects(InTarget);
	});
}

bool VoxelCoverage::HasProxyTopSurfaceInColumn(
	const FVoxelVoxelProxyData& InData,
	const int32 InX,
	const int32 InY)
{
	constexpr int32 Side = 16;
	if (InData.GridSide != Side || InData.Cells.Num() != Side * Side * Side ||
		InX < 0 || InX >= Side || InY < 0 || InY >= Side)
	{
		return false;
	}

	auto Sample = [&InData](const int32 X, const int32 Y, const int32 Z, FVoxelBlockState& OutState)
	{
		if (Z >= 0 && Z < Side)
		{
			OutState = InData.Cells[X + Y * Side + Z * Side * Side];
			return true;
		}
		if (Z == Side && InData.Known[4] && InData.Halo[4].Num() == Side * Side)
		{
			OutState = InData.Halo[4][X + Y * Side];
			return true;
		}
		return false;
	};

	for (int32 Z = 0; Z < Side; ++Z)
	{
		FVoxelBlockState Current;
		FVoxelBlockState Above;
		if (Sample(InX, InY, Z, Current) && Sample(InX, InY, Z + 1, Above) &&
			!Current.IsAir() && Above.IsAir())
		{
			return true;
		}
	}
	return false;
}
