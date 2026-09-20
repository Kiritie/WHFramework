#include "Voxel/Generation/VoxelFeature.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

uint64 FVoxelFeatureInstance::GetAllocatedBytes() const
{
	return Writes.GetAllocatedSize();
}

FVoxelFeatureAlgorithmRegistry&
FVoxelFeatureAlgorithmRegistry::Get()
{
	static FVoxelFeatureAlgorithmRegistry Instance;
	return Instance;
}

bool FVoxelFeatureAlgorithmRegistry::Register(
	TSharedRef<const IVoxelFeatureAlgorithm, ESPMode::ThreadSafe> InAlgorithm,
	FString& OutError)
{
	const FName AlgorithmId =
		InAlgorithm->GetId();

	if (AlgorithmId.IsNone())
	{
		OutError = TEXT("Voxel feature algorithm id cannot be None");
		return false;
	}

	if (InAlgorithm->GetVersion() == 0)
	{
		OutError = FString::Printf(
			TEXT("Voxel feature algorithm %s has version zero"),
			*AlgorithmId.ToString());

		return false;
	}

	FWriteScopeLock ScopeLock(Lock);

	if (Algorithms.Contains(AlgorithmId))
	{
		OutError = FString::Printf(
			TEXT("Voxel feature algorithm is already registered: %s"),
			*AlgorithmId.ToString());

		return false;
	}

	Algorithms.Add(
		AlgorithmId,
		MoveTemp(InAlgorithm));

	OutError.Reset();
	return true;
}

void FVoxelFeatureAlgorithmRegistry::Unregister(
	FName InAlgorithmId)
{
	FWriteScopeLock ScopeLock(Lock);
	Algorithms.Remove(InAlgorithmId);
}

TSharedPtr<const IVoxelFeatureAlgorithm, ESPMode::ThreadSafe>
FVoxelFeatureAlgorithmRegistry::Find(
	FName InAlgorithmId) const
{
	FReadScopeLock ScopeLock(Lock);

	const TSharedRef<
		const IVoxelFeatureAlgorithm,
		ESPMode::ThreadSafe>* Found =
			Algorithms.Find(InAlgorithmId);

	return Found
		? TSharedPtr<
			const IVoxelFeatureAlgorithm,
			ESPMode::ThreadSafe>(*Found)
		: nullptr;
}

void FVoxelFeatureAlgorithmRegistry::Reset()
{
	FWriteScopeLock ScopeLock(Lock);
	Algorithms.Reset();
}

FVoxelFeaturePlanner::FVoxelFeaturePlanner(
	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe)
	: Recipe(InRecipe)
{
}

bool FVoxelFeaturePlanner::Plan(
	const FVoxelGenerationBounds& InBounds,
	TFunctionRef<bool(
		const FIntVector& InPosition,
		FVoxelColumnSample& OutColumn)> InSampleColumn,
	TFunctionRef<bool(
		const FIntVector& InPosition,
		uint32& OutBaseSymbol)> InSampleBaseSymbol,
	TArray<FVoxelFeatureInstance>& OutInstances,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (!InBounds.IsValid())
	{
		OutError = TEXT("Voxel feature planner received invalid bounds");
		return false;
	}

	TArray<FVoxelFeatureInstance> Instances;

	for (const FVoxelFeatureRuntimeDefinition& Definition :
		Recipe->Features)
	{
		if (InCancel &&
			InCancel->Load())
		{
			OutError = TEXT("Voxel feature planning canceled");
			return false;
		}

		TSharedPtr<
			const IVoxelFeatureAlgorithm,
			ESPMode::ThreadSafe> Algorithm =
				FVoxelFeatureAlgorithmRegistry::Get().Find(
					Definition.AlgorithmId);

		if (!Algorithm)
		{
			OutError = FString::Printf(
				TEXT("Voxel feature algorithm is not registered: %s"),
				*Definition.AlgorithmId.ToString());

			return false;
		}

		if (Algorithm->GetVersion() !=
			Definition.AlgorithmVersion)
		{
			OutError = FString::Printf(
				TEXT("Voxel feature algorithm version mismatch: %s recipe=%u runtime=%u"),
				*Definition.AlgorithmId.ToString(),
				Definition.AlgorithmVersion,
				Algorithm->GetVersion());

			return false;
		}

		TArray<FIntVector> Anchors;

		GatherCandidateAnchors(
			Definition,
			InBounds,
			Anchors);

		for (const FIntVector& Anchor :
			Anchors)
		{
			if (InCancel &&
				InCancel->Load())
			{
				OutError = TEXT("Voxel feature planning canceled");
				return false;
			}

			FVoxelColumnSample Column;

			if (!InSampleColumn(
				Anchor,
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

			const int32 DefinitionIndex =
				static_cast<int32>(
					&Definition -
					Recipe->Features.GetData());

			if (!Biome.FeatureIndices.Contains(
				DefinitionIndex))
			{
				continue;
			}

			if (!IsPlacementAllowed(
				Definition,
				Anchor,
				Column))
			{
				continue;
			}

			const FIntVector SurfaceAnchor(
				Anchor.X,
				Anchor.Y,
				Definition.Placement.bRequireSurface
					? Column.SurfaceZ + 1
					: Anchor.Z);

			const uint64 CandidateSeed =
				VoxelGeneration::MakeSeed(
					Recipe->Settings.Seed,
					SurfaceAnchor,
					Definition.StableHash);

			if (VoxelGeneration::RandomRange(
					CandidateSeed,
					0,
					999) >=
				Definition.Placement.ChancePermille)
			{
				continue;
			}

			FVoxelFeatureQueryContext Context
			{
				Recipe->Settings.Seed,
				&Recipe.Get(),
				&Definition,
				InBounds,
				SurfaceAnchor,
				Column,
				InSampleColumn,
				InSampleBaseSymbol
			};

			FVoxelFeatureInstance Instance;

			if (!Algorithm->Generate(
				Context,
				Definition.ConfigBytes,
				Instance,
				OutError))
			{
				return false;
			}

			if (!Instance.Id.IsValid())
			{
				Instance.Id =
					VoxelGeneration::MakeStableId(
						Recipe->Settings.Seed,
						SurfaceAnchor,
						Definition.StableHash);
			}

			if (Instance.DefinitionId.IsNone())
			{
				Instance.DefinitionId =
					Definition.StableId;
			}

			Instance.Anchor =
				SurfaceAnchor;

			for (const FVoxelFeatureCellWrite& Write :
				Instance.Writes)
			{
				if (!InBounds.Contains(
					Write.Position))
				{
					continue;
				}

				Instances.Add(
					Instance);

				break;
			}
		}
	}

	Instances.Sort(
		[](const FVoxelFeatureInstance& InA,
		   const FVoxelFeatureInstance& InB)
		{
			return InA.Id < InB.Id;
		});

	OutInstances =
		MoveTemp(Instances);

	OutError.Reset();
	return true;
}

void FVoxelFeaturePlanner::GatherCandidateAnchors(
	const FVoxelFeatureRuntimeDefinition& InDefinition,
	const FVoxelGenerationBounds& InBounds,
	TArray<FIntVector>& OutAnchors) const
{
	OutAnchors.Reset();

	const int32 Spacing =
		FMath::Max(
			1,
			InDefinition.Placement.Spacing);

	const int32 MinGridX =
		VoxelGeneration::FloorDivide(
			InBounds.Min.X,
			Spacing);

	const int32 MaxGridX =
		VoxelGeneration::FloorDivide(
			InBounds.Max.X - 1,
			Spacing);

	const int32 MinGridY =
		VoxelGeneration::FloorDivide(
			InBounds.Min.Y,
			Spacing);

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

			const int32 OffsetX =
				VoxelGeneration::RandomRange(
					VoxelGeneration::Mix(
						Seed ^ 0x8A27A1255E012A19ull),
					0,
					Spacing - 1);

			const int32 OffsetY =
				VoxelGeneration::RandomRange(
					VoxelGeneration::Mix(
						Seed ^ 0x9125F48A3CC80177ull),
					0,
					Spacing - 1);

			OutAnchors.Add(
				FIntVector(
					GridX * Spacing + OffsetX,
					GridY * Spacing + OffsetY,
					0));
		}
	}
}

bool FVoxelFeaturePlanner::IsPlacementAllowed(
	const FVoxelFeatureRuntimeDefinition& InDefinition,
	const FIntVector& InAnchor,
	const FVoxelColumnSample& InColumn) const
{
	const FVoxelFeaturePlacement& Placement =
		InDefinition.Placement;

	const int32 CandidateZ =
		Placement.bRequireSurface
			? InColumn.SurfaceZ + 1
			: InAnchor.Z;

	if (CandidateZ < Placement.MinZ ||
		CandidateZ > Placement.MaxZ)
	{
		return false;
	}

	if (InColumn.SlopePermille >
		Placement.MaxSlopePermille)
	{
		return false;
	}

	if (!Placement.bAllowNearWater &&
		(InColumn.bRiver ||
		 InColumn.bLake ||
		 InColumn.bOcean ||
		 InColumn.bCoast))
	{
		return false;
	}

	return true;
}
