#include "Voxel/Rendering/DWVolumeTransitionPlanner.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	constexpr int32 VolumeTransitionSectionSide = 16;

	bool KeyLess(const FVoxelViewKey& A, const FVoxelViewKey& B)
	{
		if (A.Level != B.Level) return A.Level < B.Level;
		if (A.Coordinate.X != B.Coordinate.X) return A.Coordinate.X < B.Coordinate.X;
		if (A.Coordinate.Y != B.Coordinate.Y) return A.Coordinate.Y < B.Coordinate.Y;
		return A.Coordinate.Z < B.Coordinate.Z;
	}

	bool TouchesFace(const FVoxelGenerationBounds& Fine,
		const FVoxelGenerationBounds& Coarse,
		const int32 Axis, const int32 Sign)
	{
		if (Sign > 0 ? Fine.Max[Axis] != Coarse.Min[Axis]
			: Fine.Min[Axis] != Coarse.Max[Axis]) return false;
		for (int32 Other = 0; Other < 3; ++Other)
		{
			if (Other == Axis) continue;
			if (FMath::Max(Fine.Min[Other], Coarse.Min[Other]) >=
				FMath::Min(Fine.Max[Other], Coarse.Max[Other])) return false;
		}
		return true;
	}
}

void FVoxelVolumeTransitionPlanner::GatherCoverageRegions(const TSet<FVoxelViewKey>& InVisible,
	TMap<FVoxelViewKey, TSet<FVoxelViewKey>>& OutRegions)
{
	OutRegions.Reset();
	uint8 Level = 6;
	for (const auto& Key : InVisible) Level = FMath::Max(Level, Key.Level);
	TMap<FVoxelViewKey, TSet<FVoxelViewKey>> Owned;
	for (const auto& Key : InVisible)
	{
		FVoxelViewKey Region = Key;
		while (Region.Level < Level) Region = Region.GetParent();
		Owned.FindOrAdd(Region).Add(Key);
	}
	for (const auto& Pair : Owned)
	{
		auto& Local = OutRegions.Add(Pair.Key);
		Local.Append(Pair.Value);
		const FVoxelGenerationBounds Bounds = Pair.Key.GetBounds();
		for (int32 Z = -1; Z <= 1; ++Z)
		{
			for (int32 Y = -1; Y <= 1; ++Y)
			{
				for (int32 X = -1; X <= 1; ++X)
				{
					if (X == 0 && Y == 0 && Z == 0) continue;
					if (const auto* Nodes = Owned.Find({Pair.Key.Coordinate + FIntVector(X, Y, Z), Level}))
					{
						for (const FVoxelViewKey& Node : *Nodes)
						{
							const FVoxelGenerationBounds Neighbor = Node.GetBounds();
							if (Neighbor.Min.X <= Bounds.Max.X && Neighbor.Max.X >= Bounds.Min.X &&
								Neighbor.Min.Y <= Bounds.Max.Y && Neighbor.Max.Y >= Bounds.Min.Y &&
								Neighbor.Min.Z <= Bounds.Max.Z && Neighbor.Max.Z >= Bounds.Min.Z)
							{
								Local.Add(Node);
							}
						}
					}
				}
			}
		}
	}
}

void FVoxelVolumeTransitionPlanner::Build(
	const TSet<FVoxelViewKey>& InVisible,
	const uint8 InMaximumLevel,
	TArray<FVoxelVolumeTransitionFace>& OutFaces)
{
	OutFaces.Reset();
	for (const FVoxelViewKey& Fine : InVisible)
	{
		if (Fine.Level >= InMaximumLevel) continue;
		const FVoxelGenerationBounds FineBounds = Fine.GetBounds();
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (const int32 Sign : {-1, 1})
			{
				const FIntVector Point = [&]()
				{
					FIntVector Center = FineBounds.Min +
						(FineBounds.Max - FineBounds.Min) / 2;
					Center[Axis] = Sign > 0 ? FineBounds.Max[Axis]
						: FineBounds.Min[Axis] - 1;
					return Center;
				}();
				FIntVector SameCoordinate = Fine.Coordinate;
				SameCoordinate[Axis] += Sign;
				if (InVisible.Contains({SameCoordinate, Fine.Level})) continue;
				for (uint8 Level = Fine.Level + 1; Level <= InMaximumLevel; ++Level)
				{
					const int32 Side = VolumeTransitionSectionSide << Level;
					const FVoxelViewKey Coarse{
						FIntVector(VoxelGeneration::FloorDivide(Point.X, Side),
							VoxelGeneration::FloorDivide(Point.Y, Side),
							VoxelGeneration::FloorDivide(Point.Z, Side)), Level};
					if (!InVisible.Contains(Coarse) ||
						!TouchesFace(FineBounds, Coarse.GetBounds(), Axis, Sign)) continue;
					FVoxelVolumeTransitionFace& Face = OutFaces.AddDefaulted_GetRef();
					Face.Owner = Coarse;
					Face.Neighbor = Fine;
					Face.Direction = static_cast<EVoxelVolumeFaceDirection>(Axis * 2 +
						(Sign > 0 ? 1 : 0));
					Face.Min = FineBounds.Min;
					Face.Max = FineBounds.Max;
					Face.Ratio = 1 << (Level - Fine.Level);
					break;
				}
			}
		}
	}
	OutFaces.Sort([](const FVoxelVolumeTransitionFace& A,
		const FVoxelVolumeTransitionFace& B)
	{
		if (KeyLess(A.Owner, B.Owner)) return true;
		if (KeyLess(B.Owner, A.Owner)) return false;
		if (A.Direction != B.Direction)
		{
			return static_cast<uint8>(A.Direction) < static_cast<uint8>(B.Direction);
		}
		return KeyLess(A.Neighbor, B.Neighbor);
	});
}
