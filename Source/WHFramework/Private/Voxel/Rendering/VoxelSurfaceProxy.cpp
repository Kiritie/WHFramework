#include "Voxel/Rendering/VoxelSurfaceProxy.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
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

	FVoxelGenerationBounds DistantInfluenceBounds(
		const FVoxelSurfaceTileData& InData,
		const FVoxelGenerationSettings& InSettings,
		const FVoxelTreeGenerationSettings& InTrees)
	{
		const int32 TileSide = InData.GetTileSide();
		const FIntPoint Origin = InData.Key.Coordinate * TileSide;
		const int32 Crown = InTrees.bEnabled
			? FMath::Max(InData.Step, InTrees.CrownRadius) : InData.Step;
		return {
			FIntVector(Origin.X - Crown, Origin.Y - Crown, InSettings.MinZ),
			FIntVector(Origin.X + TileSide + Crown,
				Origin.Y + TileSide + Crown, InSettings.MaxZ)
		};
	}
}

FVoxelSurfaceProxyBuilder::FVoxelSurfaceProxyBuilder(
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	const FVoxelGenerationSettings& InSettings,
	const IVoxelOverlaySource& InOverlaySource,
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry)
	: Generator(InGenerator)
	, Config(InConfig)
	, Settings(InSettings)
	, OverlaySource(InOverlaySource)
	, Registry(InRegistry)
{
}

bool FVoxelSurfaceProxyBuilder::Build(
	const FVoxelSurfaceTileKey& InKey,
	FVoxelSurfaceTileData& OutData,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	FVoxelSurfaceBuildTiming* OutTiming, TArray<FVoxelColumnSample>* OutColumns) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_SurfaceBuild);

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
	const double ColumnsStart = FPlatformTime::Seconds();

	if (!Generator->SampleColumns(
		TileOrigin,
		Data.Side,
		Data.Side,
		Data.Step,
		Columns,
		OutError,
		InCancel,
		false))
	{
		return false;
	}
	const double ColumnsMilliseconds =
		(FPlatformTime::Seconds() - ColumnsStart) * 1000.0;

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

	const double OverlayStart = FPlatformTime::Seconds();
	TMap<FIntVector, FVoxelBlockState> ModifiedBlocks;
	if (!CaptureModifiedBlocks(Data, ModifiedBlocks, Data.Revision, OutError, InCancel))
	{
		return false;
	}
	if (!ApplyModifiedSurface(
		InKey,
		Data,
		ModifiedBlocks,
		OutError,
		InCancel))
	{
		return false;
	}
	if (!BuildDistantCells(Data, ModifiedBlocks, OutError, InCancel))
	{
		return false;
	}
	const double OverlayMilliseconds =
		(FPlatformTime::Seconds() - OverlayStart) * 1000.0;
	if (OutTiming)
	{
		OutTiming->ColumnsMilliseconds = ColumnsMilliseconds;
		OutTiming->OverlayMilliseconds = OverlayMilliseconds;
	}

	if (OutColumns)
	{
		*OutColumns = MoveTemp(Columns);
	}
	OutData =
		MoveTemp(Data);

	OutError.Reset();
	return true;
}

bool FVoxelSurfaceProxyBuilder::CaptureModifiedBlocks(
	const FVoxelSurfaceTileData& InData,
	TMap<FIntVector, FVoxelBlockState>& OutBlocks,
	uint64& OutRevision,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	const FVoxelTreeGenerationSettings& Trees = Config->Recipe->Settings.Ecology.Tree;
	const FVoxelGenerationBounds Bounds = DistantInfluenceBounds(InData, Settings, Trees);
	TArray<FIntVector> ModifiedSections;
	if (!OverlaySource.EnumerateModifiedSections(Bounds, ModifiedSections, InCancel))
	{
		OutError = TEXT("Canceled");
		return false;
	}
	for (const FIntVector& Section : ModifiedSections)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		FVoxelOverlaySnapshot Snapshot;
		if (!OverlaySource.ReadOverlay(Section, Snapshot))
		{
			OutError = TEXT("Distant voxel overlay could not be read");
			return false;
		}
		OutRevision = FMath::Max(OutRevision, Snapshot.Revision);
		for (const TPair<int32, FVoxelBlockState>& Edit : Snapshot.Blocks)
		{
			if (Edit.Key < 0 || Edit.Key >= SectionSide * SectionSide * SectionSide)
			{
				OutError = TEXT("Distant voxel overlay contains an invalid cell index");
				return false;
			}
			OutBlocks.Add(Section * SectionSide + UnpackCell(Edit.Key), Edit.Value);
		}
	}
	OutError.Reset();
	return true;
}

bool FVoxelSurfaceProxyBuilder::BuildDistantCells(
	FVoxelSurfaceTileData& InOutData,
	const TMap<FIntVector, FVoxelBlockState>& InModifiedBlocks,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	const int32 Step = InOutData.Step;
	const int32 Side = InOutData.GetTileSide();
	const FIntPoint Origin = InOutData.Key.Coordinate * Side;
	const FIntPoint End = Origin + FIntPoint(Side, Side);
	const FVoxelTreeGenerationSettings& Trees = Config->Recipe->Settings.Ecology.Tree;
	const FVoxelGenerationBounds Bounds = DistantInfluenceBounds(InOutData, Settings, Trees);

	int32 MinGround = MAX_int32;
	int32 MaxGround = MIN_int32;
	for (const int32 Ground : InOutData.GroundZ)
	{
		if (Ground != MIN_int32)
		{
			MinGround = FMath::Min(MinGround, Ground);
			MaxGround = FMath::Max(MaxGround, Ground);
		}
	}
	if (MinGround == MAX_int32)
	{
		MinGround = 0;
		MaxGround = 0;
	}
	const int32 StructureStep = FMath::Min(Step, 8);
	const int32 StructureSide = Side / StructureStep;
	TMap<FIntVector, FVoxelBlockState> StructureCells;
	const int32 FirstZ = VoxelGeneration::FloorDivide(
		FMath::Max(Settings.MinZ, MinGround - 16), StructureStep) * StructureStep;
	const int32 LastZ = FMath::Min(Settings.MaxZ - 1, MaxGround + 64);
	for (int64 BaseZ = FirstZ; BaseZ <= LastZ;
		BaseZ += static_cast<int64>(StructureSide) * StructureStep)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		TMap<FIntVector, FVoxelBlockState> Band;
		if (!Generator->BuildCoarseOverlay(FIntVector(Origin.X, Origin.Y,
			static_cast<int32>(BaseZ)), StructureStep, StructureSide,
			Band, OutError, InCancel))
		{
			return false;
		}
		for (const TPair<FIntVector, FVoxelBlockState>& Entry : Band)
		{
			if (Entry.Key.X >= 0 && Entry.Key.X < StructureSide &&
				Entry.Key.Y >= 0 && Entry.Key.Y < StructureSide &&
				Entry.Key.Z >= 0 && Entry.Key.Z < StructureSide)
			{
				StructureCells.Add(FIntVector(Entry.Key.X, Entry.Key.Y,
					VoxelGeneration::FloorDivide(static_cast<int32>(BaseZ) + Entry.Key.Z * StructureStep, StructureStep)),
					Entry.Value);
			}
		}
	}

	auto AddCell = [&](FIntVector Min, FIntVector Max, const FVoxelBlockState State)
	{
		Min.X = FMath::Max(Min.X, Origin.X);
		Min.Y = FMath::Max(Min.Y, Origin.Y);
		Max.X = FMath::Min(Max.X, End.X);
		Max.Y = FMath::Min(Max.Y, End.Y);
		if (!State.IsAir() && Min.X < Max.X && Min.Y < Max.Y && Min.Z < Max.Z)
		{
			InOutData.DistantCells.Add({Min, Max, State});
		}
	};
	if (Trees.bEnabled && FMath::Max(Trees.MaxHeight, Trees.CrownRadius * 2 + 1) >= Step)
	{
		FVoxelBlockState Trunk;
		FVoxelBlockState Leaves;
		if (!Config->ToRuntime(Config->Recipe->Ecology.TreeTrunk, Trunk) ||
			!Config->ToRuntime(Config->Recipe->Ecology.TreeLeaves, Leaves))
		{
			OutError = TEXT("Distant tree materials are invalid");
			return false;
		}
		if (!Generator->EnumerateTrees(Bounds,
			[&](const FIntVector& Anchor, const int32 Height)
			{
				if (FMath::Max(Height, Trees.CrownRadius * 2 + 1) < Step) return;
				const FIntVector CoarseAnchor(
					VoxelGeneration::FloorDivide(Anchor.X - Origin.X, StructureStep),
					VoxelGeneration::FloorDivide(Anchor.Y - Origin.Y, StructureStep),
					VoxelGeneration::FloorDivide(Anchor.Z, StructureStep));
				if (StructureCells.Contains(CoarseAnchor)) return;
				for (int32 Z = 0; Z < Height; ++Z)
				{
					if (const FVoxelBlockState* Edit = InModifiedBlocks.Find(Anchor + FIntVector(0, 0, Z)))
					{
						if (*Edit != Trunk) return;
					}
				}
				const int32 Radius = Trees.CrownRadius;
				const FIntVector CrownCenter = Anchor + FIntVector(0, 0, Height - 1);
				for (const TPair<FIntVector, FVoxelBlockState>& Edit : InModifiedBlocks)
				{
					const int64 DX = static_cast<int64>(Edit.Key.X) - CrownCenter.X;
					const int64 DY = static_cast<int64>(Edit.Key.Y) - CrownCenter.Y;
					const int64 DZ = static_cast<int64>(Edit.Key.Z) - CrownCenter.Z;
					if (FMath::Abs(DX) <= Radius && FMath::Abs(DY) <= Radius &&
						FMath::Abs(DZ) <= Radius &&
						DX * DX + DY * DY + DZ * DZ <= static_cast<int64>(Radius) * Radius &&
						Edit.Value != Leaves && Edit.Value != Trunk) return;
				}
				AddCell(Anchor, Anchor + FIntVector(1, 1, Height), Trunk);
				AddCell(CrownCenter - FIntVector(Radius),
					CrownCenter + FIntVector(Radius + 1), Leaves);
			}, OutError, InCancel)) return false;
	}

	struct FEditVote
	{
		int32 Removed = 0;
		TMap<uint32, int32> Placed;
		FIntVector Min = FIntVector(MAX_int32);
		FIntVector Max = FIntVector(MIN_int32);
		int32 Count = 0;
	};
	TMap<FIntVector, FEditVote> Votes;
	TSet<FIntVector> VisitedEdits;
	const FIntVector EditNeighbors[] = {
		FIntVector(1, 0, 0), FIntVector(-1, 0, 0),
		FIntVector(0, 1, 0), FIntVector(0, -1, 0),
		FIntVector(0, 0, 1), FIntVector(0, 0, -1)
	};
	auto EditClass = [this](const FVoxelBlockState& State)
	{
		if (State.IsAir()) return 1;
		const FVoxelRuntimeDefinition* Definition = Registry->Find(State.TypeId);
		return Definition && Definition->bSolid ? 2 : 0;
	};
	for (const TPair<FIntVector, FVoxelBlockState>& Edit : InModifiedBlocks)
	{
		const int32 Class = EditClass(Edit.Value);
		if (Class == 0 || VisitedEdits.Contains(Edit.Key)) continue;
		TArray<FIntVector> Component;
		Component.Add(Edit.Key);
		VisitedEdits.Add(Edit.Key);
		FIntVector Min = Edit.Key;
		FIntVector Max = Edit.Key;
		for (int32 Index = 0; Index < Component.Num(); ++Index)
		{
			if (InCancel && InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}
			const FIntVector Cell = Component[Index];
			Min.X = FMath::Min(Min.X, Cell.X);
			Min.Y = FMath::Min(Min.Y, Cell.Y);
			Min.Z = FMath::Min(Min.Z, Cell.Z);
			Max.X = FMath::Max(Max.X, Cell.X);
			Max.Y = FMath::Max(Max.Y, Cell.Y);
			Max.Z = FMath::Max(Max.Z, Cell.Z);
			for (const FIntVector& Offset : EditNeighbors)
			{
				const FIntVector Neighbor = Cell + Offset;
				const FVoxelBlockState* NeighborState = InModifiedBlocks.Find(Neighbor);
				if (NeighborState && EditClass(*NeighborState) == Class &&
					!VisitedEdits.Contains(Neighbor))
				{
					VisitedEdits.Add(Neighbor);
					Component.Add(Neighbor);
				}
			}
		}
		const int32 Span = FMath::Max3(Max.X - Min.X + 1,
			Max.Y - Min.Y + 1, Max.Z - Min.Z + 1);
		if (Span < Step) continue;
		for (const FIntVector& Cell : Component)
		{
			const FIntVector Key(
				VoxelGeneration::FloorDivide(Cell.X - Origin.X, Step),
				VoxelGeneration::FloorDivide(Cell.Y - Origin.Y, Step),
				VoxelGeneration::FloorDivide(Cell.Z, Step));
			if (Key.X < 0 || Key.X >= FVoxelSurfaceTileData::CellSide ||
				Key.Y < 0 || Key.Y >= FVoxelSurfaceTileData::CellSide) continue;
			FEditVote& Vote = Votes.FindOrAdd(Key);
			++Vote.Count;
			Vote.Min.X = FMath::Min(Vote.Min.X, Cell.X);
			Vote.Min.Y = FMath::Min(Vote.Min.Y, Cell.Y);
			Vote.Min.Z = FMath::Min(Vote.Min.Z, Cell.Z);
			Vote.Max.X = FMath::Max(Vote.Max.X, Cell.X);
			Vote.Max.Y = FMath::Max(Vote.Max.Y, Cell.Y);
			Vote.Max.Z = FMath::Max(Vote.Max.Z, Cell.Z);
			if (Class == 1) ++Vote.Removed;
			else ++Vote.Placed.FindOrAdd(InModifiedBlocks.FindChecked(Cell).Pack());
		}
	}
	TArray<FVoxelDistantCell> PlayerCells;
	TSet<FIntVector> SuppressedCells;
	for (const TPair<FIntVector, FEditVote>& Entry : Votes)
	{
		const FEditVote& Vote = Entry.Value;
		if (Vote.Count < FMath::Max(1, Step / 4)) continue;
		uint32 BestState = 0;
		int32 BestCount = 0;
		for (const TPair<uint32, int32>& Placed : Vote.Placed)
		{
			if (Placed.Value > BestCount ||
				(Placed.Value == BestCount && Placed.Key < BestState))
			{
				BestState = Placed.Key;
				BestCount = Placed.Value;
			}
		}
		if (Vote.Removed >= BestCount)
		{
			SuppressedCells.Add(Entry.Key);
		}
		else if (BestCount > Vote.Removed)
		{
			const FVoxelBlockState State = FVoxelBlockState::Unpack(BestState);
			const FVoxelRuntimeDefinition* Definition = Registry->Find(State.TypeId);
			if (Definition && Definition->bSolid)
			{
				SuppressedCells.Add(Entry.Key);
				PlayerCells.Add({Vote.Min, Vote.Max + FIntVector(1), State});
			}
		}
	}
	auto IsSuppressed = [&](const FIntVector& Key)
	{
		return SuppressedCells.Contains(FIntVector(
			VoxelGeneration::FloorDivide(Key.X * StructureStep, Step),
			VoxelGeneration::FloorDivide(Key.Y * StructureStep, Step),
			VoxelGeneration::FloorDivide(Key.Z * StructureStep, Step)));
	};
	TSet<FIntVector> VisibleStructureCells;
	if (StructureStep == Step)
	{
		for (const TPair<FIntVector, FVoxelBlockState>& Entry : StructureCells)
		{
			if (!Entry.Value.IsAir() && !IsSuppressed(Entry.Key)) VisibleStructureCells.Add(Entry.Key);
		}
	}
	else
	{
		TSet<FIntVector> Visited;
		const FIntVector Neighbors[] = {
			FIntVector(1, 0, 0), FIntVector(-1, 0, 0),
			FIntVector(0, 1, 0), FIntVector(0, -1, 0),
			FIntVector(0, 0, 1), FIntVector(0, 0, -1)
		};
		for (const TPair<FIntVector, FVoxelBlockState>& Entry : StructureCells)
		{
			if (Entry.Value.IsAir() || IsSuppressed(Entry.Key) || Visited.Contains(Entry.Key)) continue;
			TArray<FIntVector> Component{Entry.Key};
			Visited.Add(Entry.Key);
			FIntVector Min = Entry.Key;
			FIntVector Max = Entry.Key;
			for (int32 Index = 0; Index < Component.Num(); ++Index)
			{
				const FIntVector Cell = Component[Index];
				Min.X = FMath::Min(Min.X, Cell.X); Min.Y = FMath::Min(Min.Y, Cell.Y); Min.Z = FMath::Min(Min.Z, Cell.Z);
				Max.X = FMath::Max(Max.X, Cell.X); Max.Y = FMath::Max(Max.Y, Cell.Y); Max.Z = FMath::Max(Max.Z, Cell.Z);
				for (const FIntVector& Offset : Neighbors)
				{
					const FIntVector Next = Cell + Offset;
					const FVoxelBlockState* State = StructureCells.Find(Next);
					if (State && !State->IsAir() && !IsSuppressed(Next) && !Visited.Contains(Next))
					{
						Visited.Add(Next);
						Component.Add(Next);
					}
				}
			}
			const int32 Span = FMath::Max3(Max.X - Min.X + 1,
				Max.Y - Min.Y + 1, Max.Z - Min.Z + 1) * StructureStep;
			if (Span >= Step)
			{
				for (const FIntVector& Cell : Component) VisibleStructureCells.Add(Cell);
			}
		}
	}
	for (const FIntVector& Key : VisibleStructureCells)
	{
		const int32 X = Key.X;
		const int32 Y = Key.Y;
		const int32 GroundX = FMath::Min(FVoxelSurfaceTileData::CellSide,
			VoxelGeneration::FloorDivide(X * StructureStep, Step));
		const int32 GroundY = FMath::Min(FVoxelSurfaceTileData::CellSide,
			VoxelGeneration::FloorDivide(Y * StructureStep, Step));
		const int32 Ground = InOutData.GroundZ[GroundX + GroundY * InOutData.Side];
		const int32 Z = Key.Z * StructureStep;
		const int32 Bottom = Ground == MIN_int32 ? Z : FMath::Max(Z, Ground + 1);
		AddCell(FIntVector(Origin.X + X * StructureStep, Origin.Y + Y * StructureStep, Bottom),
			FIntVector(Origin.X + (X + 1) * StructureStep, Origin.Y + (Y + 1) * StructureStep, Z + StructureStep),
			StructureCells.FindChecked(Key));
	}
	for (const FVoxelDistantCell& Cell : PlayerCells)
	{
		AddCell(Cell.Min, Cell.Max, Cell.State);
	}
	InOutData.DistantCells.Sort([](const FVoxelDistantCell& A, const FVoxelDistantCell& B)
	{
		if (A.Min.X != B.Min.X) return A.Min.X < B.Min.X;
		if (A.Min.Y != B.Min.Y) return A.Min.Y < B.Min.Y;
		if (A.Min.Z != B.Min.Z) return A.Min.Z < B.Min.Z;
		if (A.Max.X != B.Max.X) return A.Max.X < B.Max.X;
		if (A.Max.Y != B.Max.Y) return A.Max.Y < B.Max.Y;
		if (A.Max.Z != B.Max.Z) return A.Max.Z < B.Max.Z;
		return A.State.Pack() < B.State.Pack();
	});
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	if (InOutData.DistantCells.Num() > 16384)
	{
		OutError = TEXT("Distant voxel tile exceeds representation capacity");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelSurfaceProxyBuilder::ApplyModifiedSurface(
	const FVoxelSurfaceTileKey& InKey,
	FVoxelSurfaceTileData& InOutData,
	const TMap<FIntVector, FVoxelBlockState>& InModifiedBlocks,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (InModifiedBlocks.IsEmpty())
	{
		return true;
	}

	TMap<
		FIntPoint,
		TMap<int32, FVoxelBlockState>>
		ColumnEdits;
	for (const TPair<FIntVector, FVoxelBlockState>& Edit : InModifiedBlocks)
	{
		ColumnEdits.FindOrAdd(FIntPoint(Edit.Key.X, Edit.Key.Y)).Add(
			Edit.Key.Z, Edit.Value);
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

			const int32 NaturalGround = InOutData.GroundZ[Index];
			int32 HighestCandidate = FMath::Max(NaturalGround, InOutData.WaterZ[Index]);
			bool bAffectsSurface = false;
			for (const TPair<int32, FVoxelBlockState>& Edit : *Edits)
			{
				if (Edit.Key < Settings.MinZ || Edit.Key >= Settings.MaxZ)
				{
					continue;
				}
				bAffectsSurface |= Edit.Key >= NaturalGround;
				HighestCandidate = FMath::Max(HighestCandidate, Edit.Key);
			}
			if (!bAffectsSurface)
			{
				continue;
			}

			// 地面和水分别解析；非实体装饰不能成为地表，清空整列不能保留旧高度。
			InOutData.GroundZ[Index] = MIN_int32;
			InOutData.WaterZ[Index] = MIN_int32;
			InOutData.SurfaceMaterial[Index] = 0;
			HighestCandidate = FMath::Min(HighestCandidate, Settings.MaxZ - 1);
			for (int32 Z = HighestCandidate; Z >= Settings.MinZ; --Z)
			{
				if (InCancel && InCancel->Load())
				{
					OutError = TEXT("Canceled");
					return false;
				}
				FVoxelBlockState State;
				if (const FVoxelBlockState* Modified = Edits->Find(Z))
				{
					State = *Modified;
				}
				else if (!Generator->SampleBlock(FIntVector(WorldXY.X, WorldXY.Y, Z), State, OutError, InCancel))
				{
					return false;
				}
				if (State.IsAir())
				{
					continue;
				}
				const FVoxelRuntimeDefinition* Definition = Registry->Find(State.TypeId);
				if (!Definition)
				{
					OutError = TEXT("Modified voxel surface contains an unknown block type");
					return false;
				}
				if (State.TypeId == Config->Water.TypeId)
				{
					InOutData.WaterZ[Index] = FMath::Max(InOutData.WaterZ[Index], Z);
					continue;
				}
				if (!Definition->bSolid)
				{
					continue;
				}
				uint32 PackedSymbol = 0;
				if (!Config->ToSymbol(State, PackedSymbol))
				{
					OutError = TEXT("Modified voxel surface material is not present in the generation recipe");
					return false;
				}
				InOutData.GroundZ[Index] = Z;
				InOutData.SurfaceMaterial[Index] = static_cast<uint16>(PackedSymbol & 0xffffu);
				break;
			}

		}
	}

	return true;
}
