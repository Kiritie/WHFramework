#include "Voxel/Rendering/VoxelWaterView.h"

bool FVoxelWaterViewBuilder::Build(
	const FVoxelSurfaceTileData& InSurface,
	FVoxelWaterSurfaceTileData& OutWater,
	FString& OutError) const
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

	FVoxelWaterSurfaceTileData Water;

	Water.Key =
		InSurface.Key;

	Water.Revision =
		InSurface.Revision;

	Water.Side =
		InSurface.Side;

	Water.Step =
		InSurface.Step;

	Water.GroundZ =
		InSurface.GroundZ;

	Water.WaterZ =
		InSurface.WaterZ;

	Water.WaterKind.Init(
		static_cast<uint8>(
			EVoxelWaterKind::None),
		Count);

	for (int32 Index = 0;
		Index < Count;
		++Index)
	{
		if (Water.WaterZ[Index] ==
				MIN_int32 ||
			Water.WaterZ[Index] <
				Water.GroundZ[Index])
		{
			continue;
		}

		const uint8 Flags =
			InSurface.Flags[Index];

		EVoxelWaterKind Kind =
			EVoxelWaterKind::None;

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

		Water.WaterKind[Index] =
			static_cast<uint8>(
				Kind);
	}

	OutWater =
		MoveTemp(Water);

	OutError.Reset();
	return true;
}
