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
