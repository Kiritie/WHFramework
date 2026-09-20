#include "Voxel/Rendering/VoxelViewTypes.h"

int32 FVoxelViewKey::GetStep() const
{
	return 1 << Level;
}

int32 FVoxelViewKey::GetSide() const
{
	return 16 * GetStep();
}

FVoxelGenerationBounds FVoxelViewKey::GetBounds() const
{
	const int32 Side = GetSide();
	const FIntVector Min = Coordinate * Side;
	return { Min, Min + FIntVector(Side) };
}

FVoxelViewKey FVoxelViewKey::GetParent() const
{
	return {
		FIntVector(
			VoxelGeneration::FloorDivide(Coordinate.X, 2),
			VoxelGeneration::FloorDivide(Coordinate.Y, 2),
			VoxelGeneration::FloorDivide(Coordinate.Z, 2)),
		static_cast<uint8>(Level + 1)
	};
}

void FVoxelViewKey::GetChildren(TArray<FVoxelViewKey>& OutChildren) const
{
	OutChildren.Reset(8);
	if (Level == 0)
	{
		return;
	}
	for (int32 Z = 0; Z < 2; ++Z)
	{
		for (int32 Y = 0; Y < 2; ++Y)
		{
			for (int32 X = 0; X < 2; ++X)
			{
				OutChildren.Add({ Coordinate * 2 + FIntVector(X, Y, Z), static_cast<uint8>(Level - 1) });
			}
		}
	}
}

bool FVoxelViewKey::operator==(const FVoxelViewKey& InOther) const
{
	return Coordinate == InOther.Coordinate && Level == InOther.Level;
}

bool FVoxelSurfaceTileKey::operator==(const FVoxelSurfaceTileKey& InOther) const
{
	return Coordinate == InOther.Coordinate && Level == InOther.Level;
}

bool FVoxelMacroTileKey::operator==(const FVoxelMacroTileKey& InOther) const
{
	return Coordinate == InOther.Coordinate && Level == InOther.Level;
}
