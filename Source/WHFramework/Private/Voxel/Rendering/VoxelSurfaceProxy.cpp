#include "Voxel/Rendering/VoxelSurfaceProxy.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	constexpr int32 SectionSide = 16;

	FIntPoint ResolveWorldXY(
		const FVoxelSurfaceTileKey& InKey,
		const int32 InX,
		const int32 InY,
		const int32 InStep)
	{
		const int32 TileSide =
			FVoxelSurfaceTileData::
				CellSide *
			InStep;

		return
			InKey.Coordinate *
				TileSide +
			FIntPoint(
				InX *
					InStep,
				InY *
					InStep);
	}

	FIntVector UnpackCell(
		const int32 InIndex)
	{
		return FIntVector(
			InIndex %
				SectionSide,
			(InIndex /
				SectionSide) %
				SectionSide,
			InIndex /
				(
					SectionSide *
					SectionSide
				));
	}
}

FVoxelSurfaceProxyBuilder::FVoxelSurfaceProxyBuilder(
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
	const FVoxelGenerationSettings& InSettings,
	const IVoxelOverlaySource& InOverlaySource)
	: Generator(InGenerator)
	, Settings(InSettings)
	, OverlaySource(InOverlaySource)
{
}

bool FVoxelSurfaceProxyBuilder::Build(
	const FVoxelSurfaceTileKey& InKey,
	FVoxelSurfaceTileData& OutData,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	FVoxelSurfaceTileData Data;

	Data.Key =
		InKey;

	Data.Side =
		FVoxelSurfaceTileData::
			VertexSide;

	Data.Step =
		1 <<
		InKey.Level;

	const int32 Count =
		Data.GetVertexCount();

	Data.GroundZ.SetNumUninitialized(Count);
	Data.WaterZ.SetNumUninitialized(Count);
	Data.SurfaceMaterial.SetNumUninitialized(Count);
	Data.Biome.SetNumUninitialized(Count);
	Data.Flags.Init(0, Count);

	const int32 TileSide =
		Data.GetTileSide();

	const FIntPoint TileOrigin =
		InKey.Coordinate *
		TileSide;

	TArray<FVoxelColumnSample> Columns;

	if (!Generator->SampleColumns(
		TileOrigin,
		Data.Side,
		Data.Side,
		Data.Step,
		Columns,
		OutError,
		InCancel))
	{
		return false;
	}

	if (Columns.Num() !=
		Count)
	{
		OutError =
			TEXT("Voxel surface column grid returned an invalid vertex count");

		return false;
	}

	for (int32 Index = 0;
		Index < Count;
		++Index)
	{
		const FVoxelColumnSample& Column =
			Columns[Index];

		Data.GroundZ[Index] =
			Column.SurfaceZ;

		Data.WaterZ[Index] =
			Column.SurfaceWaterZ;

		Data.SurfaceMaterial[Index] =
			Column.SurfaceMaterial;

		Data.Biome[Index] =
			Column.BiomeIndex;

		uint8 Flags = 0;

		if (Column.bRiver)
		{
			Flags |= VoxelSurface_River;
		}

		if (Column.bLake)
		{
			Flags |= VoxelSurface_Lake;
		}

		if (Column.bOcean)
		{
			Flags |= VoxelSurface_Ocean;
		}

		if (Column.bCoast)
		{
			Flags |= VoxelSurface_Coast;
		}

		Data.Flags[Index] =
			Flags;
	}

	if (!ApplyModifiedSurface(
		InKey,
		Data,
		OutError,
		InCancel))
	{
		return false;
	}

	OutData =
		MoveTemp(Data);

	OutError.Reset();
	return true;
}

bool FVoxelSurfaceProxyBuilder::ApplyModifiedSurface(
	const FVoxelSurfaceTileKey& InKey,
	FVoxelSurfaceTileData& InOutData,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	const int32 TileSide =
		InOutData.GetTileSide();

	const FIntPoint TileMin =
		InKey.Coordinate *
		TileSide;

	const FVoxelGenerationBounds Bounds {
		FIntVector(
			TileMin.X,
			TileMin.Y,
			MIN_int32),
		FIntVector(
			TileMin.X +
				TileSide +
				1,
			TileMin.Y +
				TileSide +
				1,
			MAX_int32)
	};

	TArray<FIntVector> ModifiedSections;

	OverlaySource.
		EnumerateModifiedSections(
			Bounds,
			ModifiedSections);

	ModifiedSections.Sort(
		[](
			const FIntVector& InA,
			const FIntVector& InB)
		{
			if (InA.Z != InB.Z)
			{
				return InA.Z >
					InB.Z;
			}

			if (InA.Y != InB.Y)
			{
				return InA.Y <
					InB.Y;
			}

			return InA.X <
				InB.X;
		});

	TMap<
		FIntPoint,
		TMap<int32, FVoxelBlockState>>
		ColumnEdits;

	for (const FIntVector& Section :
		ModifiedSections)
	{
		if (InCancel &&
			InCancel->Load())
		{
			OutError =
				TEXT("Canceled");

			return false;
		}

		FVoxelOverlaySnapshot Overlay;

		if (!OverlaySource.ReadOverlay(
			Section,
			Overlay))
		{
			OutError =
				TEXT("Modified voxel surface overlay could not be read");

			return false;
		}

		InOutData.Revision =
			FMath::Max(
				InOutData.Revision,
				Overlay.Revision);

		for (const TPair<
			int32,
			FVoxelBlockState>& Pair :
			Overlay.Blocks)
		{
			if (Pair.Key < 0 ||
				Pair.Key >=
					SectionSide *
					SectionSide *
					SectionSide)
			{
				OutError =
					TEXT("Modified voxel surface overlay contains an invalid cell index");

				return false;
			}

			const FIntVector World =
				Section *
					SectionSide +
				UnpackCell(
					Pair.Key);

			ColumnEdits.
				FindOrAdd(
					FIntPoint(
						World.X,
						World.Y)).
				Add(
					World.Z,
					Pair.Value);
		}
	}

	for (int32 Y = 0;
		Y < InOutData.Side;
		++Y)
	{
		for (int32 X = 0;
			X < InOutData.Side;
			++X)
		{
			const FIntPoint WorldXY =
				ResolveWorldXY(
					InKey,
					X,
					Y,
					InOutData.Step);

			const TMap<
				int32,
				FVoxelBlockState>* Edits =
					ColumnEdits.Find(
						WorldXY);

			if (!Edits ||
				Edits->IsEmpty())
			{
				continue;
			}

			const int32 Index =
				X +
				Y *
					InOutData.Side;

			int32 HighestCandidate =
				InOutData.GroundZ[
					Index];

			bool bAffectsSurface =
				false;

			for (const TPair<
				int32,
				FVoxelBlockState>& Edit :
				*Edits)
			{
				bAffectsSurface |=
					Edit.Key >=
					InOutData.GroundZ[
						Index];

				if (!Edit.Value.IsAir())
				{
					HighestCandidate =
						FMath::Max(
							HighestCandidate,
							Edit.Key);
				}
			}

			if (!bAffectsSurface)
			{
				continue;
			}

			const int32 ScanFloor =
				Settings.MinZ;

			for (int32 Z = HighestCandidate;
				Z >= ScanFloor;
				--Z)
			{
				FVoxelBlockState State;

				if (const FVoxelBlockState* Modified =
					Edits->Find(
						Z))
				{
					State =
						*Modified;
				}
				else if (!Generator->
					SampleBlock(
						FIntVector(
							WorldXY.X,
							WorldXY.Y,
							Z),
						State,
						OutError,
						InCancel))
				{
					return false;
				}

				if (!State.IsAir())
				{
					InOutData.GroundZ[
						Index] =
							Z;

					InOutData.SurfaceMaterial[
						Index] =
							State.TypeId;

					break;
				}
			}
		}
	}

	return true;
}
