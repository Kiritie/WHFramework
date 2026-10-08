#include "Voxel/Generation/VoxelStructure.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	FInt64Vector RotateCell(
		FInt64Vector InPosition,
		uint8 InYaw)
	{
		for (uint8 Turn = 0;
			Turn < (InYaw & 3);
			++Turn)
		{
			InPosition =
				FInt64Vector(
					-InPosition.Y - 1,
					InPosition.X,
					InPosition.Z);
		}

		return InPosition;
	}

	FInt64Vector RotateCorner(
		FInt64Vector InPosition,
		uint8 InYaw)
	{
		for (uint8 Turn = 0;
			Turn < (InYaw & 3);
			++Turn)
		{
			InPosition =
				FInt64Vector(
					-InPosition.Y,
					InPosition.X,
					InPosition.Z);
		}

		return InPosition;
	}

	bool TransformBounds(
		const FVoxelGenerationBounds& InBounds,
		const FInt64Vector& InOrigin,
		uint8 InYaw,
		FVoxelGenerationBounds& OutBounds)
	{
		if (!InBounds.IsValid())
		{
			return false;
		}
		FInt64Vector Min(MAX_int64, MAX_int64, MAX_int64);
		FInt64Vector Max(MIN_int64, MIN_int64, MIN_int64);

		for (int32 CornerIndex = 0;
			CornerIndex < 8;
			++CornerIndex)
		{
			const FInt64Vector Corner(
				(CornerIndex & 1)
					? InBounds.Max.X
					: InBounds.Min.X,
				(CornerIndex & 2)
					? InBounds.Max.Y
					: InBounds.Min.Y,
				(CornerIndex & 4)
					? InBounds.Max.Z
					: InBounds.Min.Z);

			const FInt64Vector Transformed =
				RotateCorner(
					Corner,
					InYaw) +
				InOrigin;

			Min.X =
				FMath::Min(
					Min.X,
					Transformed.X);

			Min.Y =
				FMath::Min(
					Min.Y,
					Transformed.Y);

			Min.Z =
				FMath::Min(
					Min.Z,
					Transformed.Z);

			Max.X =
				FMath::Max(
					Max.X,
					Transformed.X);

			Max.Y =
				FMath::Max(
					Max.Y,
					Transformed.Y);

			Max.Z =
				FMath::Max(
					Max.Z,
					Transformed.Z);
		}

		return VoxelGeneration::ClipWorldBounds(Min, Max, OutBounds);
	}
}

uint64 FVoxelStructureInstance::GetAllocatedBytes() const
{
	return Pieces.GetAllocatedSize() +
		Writes.GetAllocatedSize() +
		ClearVolumes.GetAllocatedSize() +
		Details.GetAllocatedSize();
}

FVoxelStructureLayoutRegistry&
FVoxelStructureLayoutRegistry::Get()
{
	static FVoxelStructureLayoutRegistry Instance;
	return Instance;
}

bool FVoxelStructureLayoutRegistry::Register(
	TSharedRef<
		const IVoxelStructureLayoutAlgorithm,
		ESPMode::ThreadSafe> InAlgorithm,
	FString& OutError)
{
	const FName AlgorithmId =
		InAlgorithm->GetId();

	if (AlgorithmId.IsNone())
	{
		OutError = TEXT("Voxel structure layout algorithm id cannot be None");
		return false;
	}

	if (InAlgorithm->GetVersion() == 0)
	{
		OutError = FString::Printf(
			TEXT("Voxel structure layout algorithm %s has version zero"),
			*AlgorithmId.ToString());

		return false;
	}

	FWriteScopeLock ScopeLock(Lock);

	if (Algorithms.Contains(AlgorithmId))
	{
		OutError = FString::Printf(
			TEXT("Voxel structure layout algorithm is already registered: %s"),
			*AlgorithmId.ToString());

		return false;
	}

	Algorithms.Add(
		AlgorithmId,
		MoveTemp(InAlgorithm));

	OutError.Reset();
	return true;
}

void FVoxelStructureLayoutRegistry::Unregister(
	FName InAlgorithmId)
{
	FWriteScopeLock ScopeLock(Lock);
	Algorithms.Remove(InAlgorithmId);
}

TSharedPtr<
	const IVoxelStructureLayoutAlgorithm,
	ESPMode::ThreadSafe>
FVoxelStructureLayoutRegistry::Find(
	FName InAlgorithmId) const
{
	FReadScopeLock ScopeLock(Lock);

	const TSharedRef<
		const IVoxelStructureLayoutAlgorithm,
		ESPMode::ThreadSafe>* Found =
			Algorithms.Find(
				InAlgorithmId);

	return Found
		? TSharedPtr<
			const IVoxelStructureLayoutAlgorithm,
			ESPMode::ThreadSafe>(*Found)
		: nullptr;
}

void FVoxelStructureLayoutRegistry::Reset()
{
	FWriteScopeLock ScopeLock(Lock);
	Algorithms.Reset();
}

FVoxelStructurePlanner::FVoxelStructurePlanner(
	TSharedRef<
		const FVoxelGenerationRecipe,
		ESPMode::ThreadSafe> InRecipe)
	: Recipe(InRecipe)
{
}

bool FVoxelStructurePlanner::Plan(
	const FVoxelGenerationBounds& InBounds,
	TFunctionRef<bool(
		const FIntVector& InPosition,
		FVoxelColumnSample& OutColumn)> InSampleColumn,
	TArray<FVoxelStructureInstance>& OutInstances,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_StructurePlan);

	if (!InBounds.IsValid())
	{
		OutError = TEXT("Voxel structure planner received invalid bounds");
		return false;
	}

	TArray<FVoxelStructureInstance> Instances;

	for (int32 DefinitionIndex = 0;
		DefinitionIndex < Recipe->Structures.Num();
		++DefinitionIndex)
	{
		if (InCancel &&
			InCancel->Load())
		{
			OutError = TEXT("Voxel structure planning canceled");
			return false;
		}

		const FVoxelStructureRuntimeDefinition& Definition =
			Recipe->Structures[DefinitionIndex];

		TArray<FIntVector> Candidates;

		GatherCandidates(
			Definition,
			InBounds,
			Candidates);

		for (const FIntVector& Candidate :
			Candidates)
		{
			if (InCancel &&
				InCancel->Load())
			{
				OutError = TEXT("Voxel structure planning canceled");
				return false;
			}

			FVoxelColumnSample Column;

			if (!InSampleColumn(
				Candidate,
				Column))
			{
				continue;
			}

			if (!Recipe->Biomes.IsValidIndex(
				Column.BiomeIndex))
			{
				continue;
			}

			const FVoxelBiomeRuntimeDefinition& Biome =
				Recipe->Biomes[
					Column.BiomeIndex];

			if (!Biome.StructureIndices.Contains(
				DefinitionIndex))
			{
				continue;
			}

			if (Column.SlopePermille >
				Definition.Placement.MaxSlopePermille)
			{
				continue;
			}

			const int64 CandidateZ =
				Definition.Placement.bRequireSurface
					? int64(Column.SurfaceZ) + 1
					: Candidate.Z;

			if (!VoxelGeneration::IsWorldCell(CandidateZ) || CandidateZ <
					Definition.Placement.MinZ ||
				CandidateZ >
					Definition.Placement.MaxZ)
			{
				continue;
			}

			FIntVector Anchor;
			if (!ResolveTerrainAdaptedAnchor(
					Definition,
					FIntVector(
						Candidate.X,
						Candidate.Y,
						int32(CandidateZ)),
					Column, Anchor))
			{
				continue;
			}

			const uint64 CandidateSeed =
				VoxelGeneration::MakeSeed(
					Recipe->Settings.Seed,
					Anchor,
					Definition.StableHash);

			if (VoxelGeneration::RandomRange(
				CandidateSeed,
				0,
				999) >=
				Definition.Placement.ChancePermille)
			{
				continue;
			}

			FVoxelStructureInstance Instance;

			if (!BuildInstance(
				DefinitionIndex,
					Definition,
					Anchor,
					Instance,
				OutError))
			{
				return false;
			}

			bool bTouchesQuery =
				false;

			for (const FVoxelStructureCellWrite& Write :
				Instance.Writes)
			{
				if (InBounds.Contains(
					Write.Position))
				{
					bTouchesQuery =
						true;

					break;
				}
			}

			if (!bTouchesQuery)
			{
				for (const FVoxelStructureClearVolume& ClearVolume :
					Instance.ClearVolumes)
				{
					if (ClearVolume.Bounds.Intersects(
						InBounds))
					{
						bTouchesQuery =
							true;

						break;
					}
				}
			}

			if (bTouchesQuery)
			{
				Instances.Add(
					MoveTemp(Instance));
			}
		}
	}

	Instances.Sort(
		[](const FVoxelStructureInstance& InA,
		   const FVoxelStructureInstance& InB)
		{
			return InA.Id < InB.Id;
		});

	OutInstances =
		MoveTemp(Instances);

	OutError.Reset();
	return true;
}

bool FVoxelStructurePlanner::BuildAt(
	const FVoxelPlannedStructurePlacement& InPlacement,
	FVoxelStructureInstance& OutInstance,
	FString& OutError) const
{
	if (!InPlacement.IsValid())
	{
		OutError = TEXT("Planned voxel structure placement is invalid");
		return false;
	}
	const int32 DefinitionIndex = Recipe->FindStructure(InPlacement.DefinitionId);
	if (!Recipe->Structures.IsValidIndex(DefinitionIndex))
	{
		OutError = FString::Printf(TEXT("Unknown planned voxel structure definition: %s"),
			*InPlacement.DefinitionId.ToString());
		return false;
	}
	return BuildInstance(
		DefinitionIndex,
		Recipe->Structures[DefinitionIndex],
		InPlacement.Anchor,
		OutInstance,
		OutError,
		InPlacement.Id,
		InPlacement.Yaw);
}

void FVoxelStructurePlanner::GatherCandidates(
	const FVoxelStructureRuntimeDefinition& InDefinition,
	const FVoxelGenerationBounds& InBounds,
	TArray<FIntVector>& OutCandidates) const
{
	OutCandidates.Reset();

	const int32 Spacing =
		FMath::Max(
			1,
			InDefinition.Placement.Spacing);

	const int32 Separation =
		FMath::Clamp(
			InDefinition.Placement.Separation,
			0,
			FMath::Max(
				0,
				Spacing / 2 - 1));

	const int64 MinGridX =
		VoxelGeneration::FloorDivideWide(
			InBounds.Min.X,
			Spacing) - 1;

	const int64 MaxGridX =
		VoxelGeneration::FloorDivideWide(
			InBounds.Max.X - 1,
			Spacing);

	const int64 MinGridY =
		VoxelGeneration::FloorDivideWide(
			InBounds.Min.Y,
			Spacing) - 1;

	const int64 MaxGridY =
		VoxelGeneration::FloorDivideWide(
			InBounds.Max.Y - 1,
			Spacing);

	for (int64 GridY = MinGridY;
		GridY <= MaxGridY;
		++GridY)
	{
		for (int64 GridX = MinGridX;
			GridX <= MaxGridX;
			++GridX)
		{
			if (GridX < MIN_int32 || GridX > MAX_int32 || GridY < MIN_int32 || GridY > MAX_int32)
			{
				continue;
			}
			const FIntVector Grid(
				int32(GridX),
				int32(GridY),
				0);

			const uint64 Seed =
				VoxelGeneration::MakeSeed(
					Recipe->Settings.Seed,
					Grid,
					InDefinition.StableHash);

			const int32 MinOffset =
				Separation;

			const int32 MaxOffset =
				FMath::Max(
					MinOffset,
					Spacing -
					Separation -
					1);

			const int32 OffsetX =
				VoxelGeneration::RandomRange(
					VoxelGeneration::Mix(
						Seed ^ 0x62AB913E00F49217ull),
					MinOffset,
					MaxOffset);

			const int32 OffsetY =
				VoxelGeneration::RandomRange(
					VoxelGeneration::Mix(
						Seed ^ 0xE2F20D0948C3A115ull),
					MinOffset,
					MaxOffset);

			FIntVector Candidate;
			if (!VoxelGeneration::TryWorldCell(FInt64Vector(GridX * Spacing + OffsetX,
				GridY * Spacing + OffsetY, 0), Candidate))
			{
				continue;
			}

			if (Candidate.X < InBounds.Min.X ||
				Candidate.Y < InBounds.Min.Y ||
				Candidate.X >= InBounds.Max.X ||
				Candidate.Y >= InBounds.Max.Y)
			{
				continue;
			}

			OutCandidates.Add(Candidate);
		}
	}
}

bool FVoxelStructurePlanner::BuildInstance(
	int32 InDefinitionIndex,
	const FVoxelStructureRuntimeDefinition& InDefinition,
	const FIntVector& InCandidate,
	FVoxelStructureInstance& OutInstance,
	FString& OutError,
	TOptional<FVoxelStableId> InId,
	TOptional<uint8> InRootYaw) const
{
	if (!VoxelGeneration::IsWorldCell(InCandidate.X) || !VoxelGeneration::IsWorldCell(InCandidate.Y) ||
		!VoxelGeneration::IsWorldCell(InCandidate.Z))
	{
		OutError = TEXT("CoordinateOutOfRange: voxel structure anchor");
		return false;
	}
	const uint64 CandidateSeed =
		VoxelGeneration::MakeSeed(
			Recipe->Settings.Seed,
			InCandidate,
			InDefinition.StableHash);

	TArray<FVoxelStructurePiecePlacement> Pieces;

	if (!BuildPiecePlacements(
		InDefinition,
		InCandidate,
		CandidateSeed,
		Pieces,
		OutError))
	{
		return false;
	}

	// 超过全部 int32 模板与世界坐标可抵消范围的偏移不可能产生合法 cell。
	constexpr int64 MaximumLocalOffset = 6ll * (int64(MAX_int32) + 1);
	for (const FVoxelStructurePiecePlacement& Placement : Pieces)
	{
		if (Placement.Origin.X < -MaximumLocalOffset || Placement.Origin.X > MaximumLocalOffset ||
			Placement.Origin.Y < -MaximumLocalOffset || Placement.Origin.Y > MaximumLocalOffset ||
			Placement.Origin.Z < -MaximumLocalOffset || Placement.Origin.Z > MaximumLocalOffset)
		{
			OutError = TEXT("CoordinateOutOfRange: voxel structure local origin");
			return false;
		}
	}

	if (InRootYaw.IsSet())
	{
		for (FVoxelStructurePiecePlacement& Placement : Pieces)
		{
			if (InDefinition.LayoutAlgorithmId.IsNone())
			{
				Placement.Yaw = InRootYaw.GetValue();
			}
			else
			{
				Placement.Origin = RotateCorner(Placement.Origin, InRootYaw.GetValue());
				Placement.Yaw = (Placement.Yaw + InRootYaw.GetValue()) & 3;
			}
		}
	}

	FVoxelStructureInstance Instance;

	Instance.Id = InId.IsSet()
		? InId.GetValue()
		: VoxelGeneration::MakeStableId(
			Recipe->Settings.Seed,
			InCandidate,
			InDefinition.StableHash,
			static_cast<uint64>(
				InDefinitionIndex));

	Instance.DefinitionId =
		InDefinition.StableId;

	Instance.Anchor =
		InCandidate;

	Instance.Pieces =
		Pieces;

	for (const FVoxelStructurePiecePlacement& Placement :
		Pieces)
	{
		if (!RasterizePiece(
			InDefinition,
			Placement,
			InCandidate,
			Instance.Writes,
			Instance.ClearVolumes,
			OutError))
		{
			return false;
		}

		const FVoxelStructurePieceTemplate& Piece =
			InDefinition.Pieces[Placement.PieceIndex];
		const FInt64Vector PieceOrigin =
			FInt64Vector(InCandidate) + Placement.Origin - RotateCorner(FInt64Vector(Piece.Entrance), Placement.Yaw);
		for (const FVoxelStructureDetailRuntimeSocket& Socket : Piece.Details)
		{
			FVoxelStructureDetailPlacement Detail;
			Detail.DetailId = Socket.DetailId;
			if (!VoxelGeneration::TryWorldCell(PieceOrigin + RotateCorner(FInt64Vector(Socket.SourceCorner), Placement.Yaw),
				Detail.Position))
			{
				continue;
			}
			Detail.Yaw = (Socket.Yaw + Placement.Yaw) & 3;
			Detail.OwnerId = Instance.Id;
			Instance.Details.Add(MoveTemp(Detail));
		}
	}

	/**
	 * 稳定排序。
	 * 后续多 Structure 冲突解决时必须使用：
	 *
	 * Stage -> InstanceId -> Position
	 */
	Instance.Writes.Sort(
		[](const FVoxelStructureCellWrite& InA,
		   const FVoxelStructureCellWrite& InB)
		{
			if (InA.Position.Z != InB.Position.Z)
			{
				return InA.Position.Z < InB.Position.Z;
			}

			if (InA.Position.Y != InB.Position.Y)
			{
				return InA.Position.Y < InB.Position.Y;
			}

			return InA.Position.X < InB.Position.X;
		});

	OutInstance =
		MoveTemp(Instance);

	OutError.Reset();
	return true;
}

bool FVoxelStructurePlanner::BuildPiecePlacements(
	const FVoxelStructureRuntimeDefinition& InDefinition,
	const FIntVector& InAnchor,
	uint64 InCandidateSeed,
	TArray<FVoxelStructurePiecePlacement>& OutPieces,
	FString& OutError) const
{
	OutPieces.Reset();

	if (!InDefinition.LayoutAlgorithmId.IsNone())
	{
		TSharedPtr<
			const IVoxelStructureLayoutAlgorithm,
			ESPMode::ThreadSafe> Algorithm =
				FVoxelStructureLayoutRegistry::Get().Find(
					InDefinition.LayoutAlgorithmId);

		if (!Algorithm)
		{
			OutError = FString::Printf(
				TEXT("Voxel structure layout algorithm is not registered: %s"),
				*InDefinition.LayoutAlgorithmId.ToString());

			return false;
		}

		if (Algorithm->GetVersion() !=
			InDefinition.LayoutAlgorithmVersion)
		{
			OutError = FString::Printf(
				TEXT("Voxel structure layout algorithm version mismatch: %s recipe=%u runtime=%u"),
				*InDefinition.LayoutAlgorithmId.ToString(),
				InDefinition.LayoutAlgorithmVersion,
				Algorithm->GetVersion());

			return false;
		}

		FVoxelStructureLayoutContext Context;
		Context.WorldSeed =
			Recipe->Settings.Seed;

		Context.Recipe =
			&Recipe.Get();

		Context.Definition =
			&InDefinition;

		Context.Anchor =
			InAnchor;

		Context.CandidateSeed =
			InCandidateSeed;

		return Algorithm->BuildLayout(
			Context,
			OutPieces,
			OutError);
	}

	if (InDefinition.Pieces.IsEmpty())
	{
		OutError = TEXT("Voxel structure has no pieces");
		return false;
	}

	FVoxelStructurePiecePlacement Placement;

	Placement.PieceIndex = 0;
	Placement.Origin =
		FInt64Vector::ZeroValue;

	Placement.Yaw =
		static_cast<uint8>(
			VoxelGeneration::RandomRange(
				VoxelGeneration::Mix(
					InCandidateSeed ^
					0xE5A1B9913CA70281ull),
				0,
				3));

	OutPieces.Add(
		Placement);

	OutError.Reset();
	return true;
}

bool FVoxelStructurePlanner::RasterizePiece(
	const FVoxelStructureRuntimeDefinition& InDefinition,
	const FVoxelStructurePiecePlacement& InPlacement,
	const FIntVector& InStructureAnchor,
	TArray<FVoxelStructureCellWrite>& OutWrites,
	TArray<FVoxelStructureClearVolume>& OutClearVolumes,
	FString& OutError) const
{
	if (!InDefinition.Pieces.IsValidIndex(
		InPlacement.PieceIndex))
	{
		OutError = TEXT("Voxel structure piece placement references an invalid piece");
		return false;
	}

	const FVoxelStructurePieceTemplate& Piece =
		InDefinition.Pieces[
			InPlacement.PieceIndex];

	const FInt64Vector PieceOrigin =
		FInt64Vector(InStructureAnchor) +
		InPlacement.Origin -
		RotateCorner(
			FInt64Vector(Piece.Entrance),
			InPlacement.Yaw);

	for (const FVoxelStructureRun& Run :
		Piece.Writes)
	{
		for (int32 RunOffset = 0;
			RunOffset < Run.Length;
			++RunOffset)
		{
			const FInt64Vector LocalCell =
				FInt64Vector(Run.Start) +
				FInt64Vector(
					RunOffset,
					0,
					0);

			FVoxelStructureCellWrite Write;

			if (!VoxelGeneration::TryWorldCell(PieceOrigin +
				RotateCell(
					LocalCell,
					InPlacement.Yaw), Write.Position))
			{
				continue;
			}

			Write.Value =
				Run.Value;

			OutWrites.Add(
				MoveTemp(Write));
		}
	}

	for (const FVoxelGenerationBounds& ClearVolume :
		Piece.ClearVolumes)
	{
		FVoxelStructureClearVolume RuntimeClear;

		if (!TransformBounds(
				ClearVolume,
				PieceOrigin,
				InPlacement.Yaw, RuntimeClear.Bounds))
		{
			continue;
		}

		OutClearVolumes.Add(
			MoveTemp(RuntimeClear));
	}

	OutError.Reset();
	return true;
}

bool FVoxelStructurePlanner::ResolveTerrainAdaptedAnchor(
	const FVoxelStructureRuntimeDefinition& InDefinition,
	const FIntVector& InCandidate,
	const FVoxelColumnSample& InColumn,
	FIntVector& OutAnchor) const
{
	FInt64Vector Result(InCandidate);

	switch (InDefinition.Adaptation)
	{
	case EVoxelStructureAdaptation::Bury:
		Result.Z -=
			InDefinition.Placement.MaxTerrainAdjustment;
		break;

	case EVoxelStructureAdaptation::Flatten:
	case EVoxelStructureAdaptation::CutFill:
	case EVoxelStructureAdaptation::Blend:
	case EVoxelStructureAdaptation::Encapsulate:
	case EVoxelStructureAdaptation::None:
	default:
		break;
	}

	return VoxelGeneration::TryWorldCell(Result, OutAnchor);
}
