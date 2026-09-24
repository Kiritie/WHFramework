#include "Voxel/Generation/VoxelStructure.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	FIntVector RotateCell(
		FIntVector InPosition,
		uint8 InYaw)
	{
		for (uint8 Turn = 0;
			Turn < (InYaw & 3);
			++Turn)
		{
			InPosition =
				FIntVector(
					-InPosition.Y - 1,
					InPosition.X,
					InPosition.Z);
		}

		return InPosition;
	}

	FIntVector RotateCorner(
		FIntVector InPosition,
		uint8 InYaw)
	{
		for (uint8 Turn = 0;
			Turn < (InYaw & 3);
			++Turn)
		{
			InPosition =
				FIntVector(
					-InPosition.Y,
					InPosition.X,
					InPosition.Z);
		}

		return InPosition;
	}

	FVoxelGenerationBounds TransformBounds(
		const FVoxelGenerationBounds& InBounds,
		const FIntVector& InOrigin,
		uint8 InYaw)
	{
		FVoxelGenerationBounds Result;

		Result.Min =
			FIntVector(
				MAX_int32,
				MAX_int32,
				MAX_int32);

		Result.Max =
			FIntVector(
				MIN_int32,
				MIN_int32,
				MIN_int32);

		for (int32 CornerIndex = 0;
			CornerIndex < 8;
			++CornerIndex)
		{
			const FIntVector Corner(
				(CornerIndex & 1)
					? InBounds.Max.X
					: InBounds.Min.X,
				(CornerIndex & 2)
					? InBounds.Max.Y
					: InBounds.Min.Y,
				(CornerIndex & 4)
					? InBounds.Max.Z
					: InBounds.Min.Z);

			const FIntVector Transformed =
				RotateCorner(
					Corner,
					InYaw) +
				InOrigin;

			Result.Min.X =
				FMath::Min(
					Result.Min.X,
					Transformed.X);

			Result.Min.Y =
				FMath::Min(
					Result.Min.Y,
					Transformed.Y);

			Result.Min.Z =
				FMath::Min(
					Result.Min.Z,
					Transformed.Z);

			Result.Max.X =
				FMath::Max(
					Result.Max.X,
					Transformed.X);

			Result.Max.Y =
				FMath::Max(
					Result.Max.Y,
					Transformed.Y);

			Result.Max.Z =
				FMath::Max(
					Result.Max.Z,
					Transformed.Z);
		}

		return Result;
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

			const int32 CandidateZ =
				Definition.Placement.bRequireSurface
					? Column.SurfaceZ + 1
					: Candidate.Z;

			if (CandidateZ <
					Definition.Placement.MinZ ||
				CandidateZ >
					Definition.Placement.MaxZ)
			{
				continue;
			}

			const FIntVector Anchor =
				ResolveTerrainAdaptedAnchor(
					Definition,
					FIntVector(
						Candidate.X,
						Candidate.Y,
						CandidateZ),
					Column);

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

	const int32 MinGridX =
		VoxelGeneration::FloorDivide(
			InBounds.Min.X,
			Spacing) - 1;

	const int32 MaxGridX =
		VoxelGeneration::FloorDivide(
			InBounds.Max.X - 1,
			Spacing);

	const int32 MinGridY =
		VoxelGeneration::FloorDivide(
			InBounds.Min.Y,
			Spacing) - 1;

	const int32 MaxGridY =
		VoxelGeneration::FloorDivide(
			InBounds.Max.Y - 1,
			Spacing);

	for (int32 GridY = MinGridY;
		GridY <= MaxGridY;
		++GridY)
	{
		for (int32 GridX = MinGridX;
			GridX <= MaxGridX;
			++GridX)
		{
			const FIntVector Grid(
				GridX,
				GridY,
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

			const FIntVector Candidate(
				GridX * Spacing + OffsetX,
				GridY * Spacing + OffsetY,
				0);

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
		const FIntVector PieceOrigin =
			InCandidate + Placement.Origin - RotateCorner(Piece.Entrance, Placement.Yaw);
		for (const FVoxelStructureDetailRuntimeSocket& Socket : Piece.Details)
		{
			FVoxelStructureDetailPlacement Detail;
			Detail.DetailId = Socket.DetailId;
			Detail.Position = PieceOrigin + RotateCorner(Socket.SourceCorner, Placement.Yaw);
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
		FIntVector::ZeroValue;

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

	const FIntVector PieceOrigin =
		InStructureAnchor +
		InPlacement.Origin -
		RotateCorner(
			Piece.Entrance,
			InPlacement.Yaw);

	for (const FVoxelStructureRun& Run :
		Piece.Writes)
	{
		for (int32 RunOffset = 0;
			RunOffset < Run.Length;
			++RunOffset)
		{
			const FIntVector LocalCell =
				Run.Start +
				FIntVector(
					RunOffset,
					0,
					0);

			FVoxelStructureCellWrite Write;

			Write.Position =
				PieceOrigin +
				RotateCell(
					LocalCell,
					InPlacement.Yaw);

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

		RuntimeClear.Bounds =
			TransformBounds(
				ClearVolume,
				PieceOrigin,
				InPlacement.Yaw);

		OutClearVolumes.Add(
			MoveTemp(RuntimeClear));
	}

	OutError.Reset();
	return true;
}

FIntVector FVoxelStructurePlanner::ResolveTerrainAdaptedAnchor(
	const FVoxelStructureRuntimeDefinition& InDefinition,
	const FIntVector& InCandidate,
	const FVoxelColumnSample& InColumn) const
{
	FIntVector Result =
		InCandidate;

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

	return Result;
}
