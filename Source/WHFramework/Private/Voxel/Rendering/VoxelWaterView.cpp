#include "Voxel/Rendering/VoxelWaterView.h"

bool FVoxelWaterViewBuilder::BuildKindGrid(
	const FVoxelSurfaceTileData& InSurface,
	TArray<uint8>& OutKinds,
	FString& OutError)
{
	const int32 Count =
		InSurface.Side *
		InSurface.Side;

	if (InSurface.Side < 2 ||
		InSurface.Step <= 0 ||
		InSurface.GroundZ.Num() !=
			Count ||
		InSurface.WaterZ.Num() !=
			Count ||
		InSurface.Flags.Num() !=
			Count)
	{
		OutError =
			TEXT("Voxel water input surface tile is invalid");

		return false;
	}

	OutKinds.Init(
		static_cast<uint8>(
			EVoxelWaterKind::None),
		Count);

	for (int32 Index = 0;
		Index < Count;
		++Index)
	{
		if (InSurface.WaterZ[Index] ==
				MIN_int32 ||
			InSurface.WaterZ[Index] <
				InSurface.GroundZ[Index])
		{
			continue;
		}

		const uint8 Flags =
			InSurface.Flags[Index];

		EVoxelWaterKind Kind =
			EVoxelWaterKind::Edited;

		if ((Flags &
			VoxelSurface_Ocean) != 0)
		{
			Kind =
				EVoxelWaterKind::Ocean;
		}
		else if ((Flags &
			VoxelSurface_Lake) != 0)
		{
			Kind =
				EVoxelWaterKind::Lake;
		}
		else if ((Flags &
			VoxelSurface_River) != 0)
		{
			Kind =
				EVoxelWaterKind::River;
		}

		OutKinds[Index] =
			static_cast<uint8>(
				Kind);
	}
	OutError.Reset();
	return true;
}

bool FVoxelWaterViewBuilder::Build(
	const FVoxelSurfaceTileData& InSurface,
	FVoxelWaterSurfaceTileData& OutWater,
	FString& OutError) const
{
	FVoxelWaterSurfaceTileData Water;
	if (!BuildKindGrid(InSurface, Water.WaterKind, OutError))
	{
		return false;
	}
	Water.Key = InSurface.Key;
	Water.Revision = InSurface.Revision;
	Water.Side = InSurface.Side;
	Water.Step = InSurface.Step;
	Water.GroundZ = InSurface.GroundZ;
	Water.WaterZ = InSurface.WaterZ;

	OutWater =
		MoveTemp(Water);

	OutError.Reset();
	return true;
}
