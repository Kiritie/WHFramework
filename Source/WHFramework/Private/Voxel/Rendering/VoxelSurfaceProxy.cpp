#include "Voxel/Rendering/VoxelSurfaceProxy.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

namespace VoxelSurfaceProxyPrivate
{
	constexpr int32 SurfaceGridSide = 32;
	constexpr int32 SectionSide = 16;

	FIntPoint ResolveWorldXY(
		const FVoxelSurfaceTileKey& InKey,
		const int32 InX,
		const int32 InY,
		const int32 InStep)
	{
		const int32 TileSide = SurfaceGridSide * InStep;
		return InKey.Coordinate * TileSide + FIntPoint(InX * InStep, InY * InStep);
	}

	FIntVector UnpackCell(const int32 InIndex)
	{
		return FIntVector(
			InIndex % SectionSide,
			(InIndex / SectionSide) % SectionSide,
			InIndex / (SectionSide * SectionSide));
	}
}

using namespace VoxelSurfaceProxyPrivate;

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
	Data.Key = InKey;
	Data.Side = SurfaceGridSide;
	Data.Step = 1 << InKey.Level;
	const int32 Count = Data.Side * Data.Side;
	Data.GroundZ.SetNumUninitialized(Count);
	Data.WaterZ.SetNumUninitialized(Count);
	Data.SurfaceMaterial.SetNumUninitialized(Count);
	Data.Biome.SetNumUninitialized(Count);
	Data.Flags.Init(0, Count);

	for (int32 Y = 0; Y < Data.Side; ++Y)
	{
		for (int32 X = 0; X < Data.Side; ++X)
		{
			if (InCancel && InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}
			const FIntPoint WorldXY = ResolveWorldXY(InKey, X, Y, Data.Step);
			FVoxelColumnSample Column;
			if (!Generator->SampleColumn(WorldXY.X, WorldXY.Y, Column, OutError, InCancel))
			{
				return false;
			}

			const int32 Index = X + Y * Data.Side;
			Data.GroundZ[Index] = Column.SurfaceZ;
			Data.WaterZ[Index] = Column.SurfaceWaterZ;
			Data.Biome[Index] = Column.BiomeIndex;
			Data.SurfaceMaterial[Index] = Column.SurfaceMaterial;
			uint8 Flags = 0;
			Flags |= Column.bRiver ? VoxelSurface_River : 0;
			Flags |= Column.bLake ? VoxelSurface_Lake : 0;
			Flags |= Column.bOcean ? VoxelSurface_Ocean : 0;
			Flags |= Column.bCoast ? VoxelSurface_Coast : 0;
			Data.Flags[Index] = Flags;
		}
	}

	if (!ApplyModifiedSurface(InKey, Data, OutError, InCancel))
	{
		return false;
	}
	OutData = MoveTemp(Data);
	OutError.Reset();
	return true;
}

bool FVoxelSurfaceProxyBuilder::ApplyModifiedSurface(
	const FVoxelSurfaceTileKey& InKey,
	FVoxelSurfaceTileData& InOutData,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	const int32 TileSide = InOutData.Side * InOutData.Step;
	const FIntPoint TileMin = InKey.Coordinate * TileSide;
	const FVoxelGenerationBounds Bounds {
		FIntVector(TileMin.X, TileMin.Y, Settings.MinZ),
		FIntVector(TileMin.X + TileSide, TileMin.Y + TileSide, Settings.MaxZ)
	};
	TArray<FIntVector> ModifiedSections;
	OverlaySource.EnumerateModifiedSections(Bounds, ModifiedSections);
	ModifiedSections.Sort([](const FIntVector& InA, const FIntVector& InB)
	{
		if (InA.Z != InB.Z)
		{
			return InA.Z > InB.Z;
		}
		if (InA.Y != InB.Y)
		{
			return InA.Y < InB.Y;
		}
		return InA.X < InB.X;
	});

	TMap<FIntPoint, TMap<int32, FVoxelBlockState>> ColumnEdits;
	for (const FIntVector& Section : ModifiedSections)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		FVoxelOverlaySnapshot Overlay;
		if (!OverlaySource.ReadOverlay(Section, Overlay))
		{
			OutError = TEXT("Modified voxel surface overlay could not be read");
			return false;
		}
		InOutData.Revision = FMath::Max(InOutData.Revision, Overlay.Revision);
		for (const TPair<int32, FVoxelBlockState>& Pair : Overlay.Blocks)
		{
			if (Pair.Key < 0 || Pair.Key >= SectionSide * SectionSide * SectionSide)
			{
				OutError = TEXT("Modified voxel surface overlay contains an invalid cell index");
				return false;
			}
			const FIntVector World = Section * SectionSide + UnpackCell(Pair.Key);
			ColumnEdits.FindOrAdd(FIntPoint(World.X, World.Y)).Add(World.Z, Pair.Value);
		}
	}

	for (int32 Y = 0; Y < InOutData.Side; ++Y)
	{
		for (int32 X = 0; X < InOutData.Side; ++X)
		{
			const FIntPoint WorldXY = ResolveWorldXY(InKey, X, Y, InOutData.Step);
			const TMap<int32, FVoxelBlockState>* Edits = ColumnEdits.Find(WorldXY);
			if (!Edits || Edits->IsEmpty())
			{
				continue;
			}

			const int32 Index = X + Y * InOutData.Side;
			int32 HighestCandidate = InOutData.GroundZ[Index];
			for (const TPair<int32, FVoxelBlockState>& Edit : *Edits)
			{
				if (!Edit.Value.IsAir())
				{
					HighestCandidate = FMath::Max(HighestCandidate, Edit.Key);
				}
			}

			int32 LowestTouched = MAX_int32;
			for (const TPair<int32, FVoxelBlockState>& Edit : *Edits)
			{
				LowestTouched = FMath::Min(LowestTouched, Edit.Key);
			}
			const int32 ScanFloor = VoxelGeneration::FloorDivide(LowestTouched, SectionSide) * SectionSide - 1;
			for (int32 Z = HighestCandidate; Z >= ScanFloor; --Z)
			{
				FVoxelBlockState State;
				if (const FVoxelBlockState* Modified = Edits->Find(Z))
				{
					State = *Modified;
				}
				else if (!Generator->SampleBlock(FIntVector(WorldXY.X, WorldXY.Y, Z), State, OutError, InCancel))
				{
					return false;
				}
				if (!State.IsAir())
				{
					InOutData.GroundZ[Index] = Z;
					InOutData.SurfaceMaterial[Index] = State.TypeId;
					break;
				}
			}
		}
	}
	return true;
}
