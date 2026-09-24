#include "Voxel/Generation/VoxelGenerationQuery.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/Aquifer/VoxelAquiferGenerator.h"
#include "Voxel/Generation/Biome/VoxelBiomeGenerator.h"
#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyPlanner.h"
#include "Voxel/Generation/Surface/VoxelSurfaceGenerator.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/VoxelFeature.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelStructure.h"

namespace
{
	constexpr int32 VoxelGenerationPlanTileSide = 256;

	FVoxelGenerationBounds MakeTileBounds(
		const FVoxelGenerationTileKey& InKey)
	{
		const FIntVector Min =
			InKey.Coordinate *
			VoxelGenerationPlanTileSide;

		return {
			Min,
			Min +
				FIntVector(
					VoxelGenerationPlanTileSide)
		};
	}
}

bool FVoxelGenerationQuery::Create(
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
	FVoxelGenerationQuery& OutQuery,
	FString& OutError,
	const bool bInUseColumnCache)
{
	if (!InConfig->IsValid() ||
		!InConfig->Recipe)
	{
		OutError =
			TEXT("Voxel generation query received an invalid runtime config");

		return false;
	}

	FVoxelGenerationQuery Query;

	Query.Config = InConfig;
	Query.Cache = InCache;
	Query.bUseColumnCache = bInUseColumnCache;

	Query.Climate =
		MakeShared<
			FVoxelClimateGenerator,
			ESPMode::ThreadSafe>(
				InConfig->Recipe.
					ToSharedRef());

	Query.Terrain =
		MakeShared<
			FVoxelTerrainGenerator,
			ESPMode::ThreadSafe>(
				InConfig->Recipe.
					ToSharedRef(),
				Query.Climate.
					ToSharedRef());

	Query.Biome =
		MakeShared<
			FVoxelBiomeGenerator,
			ESPMode::ThreadSafe>(
				InConfig->Recipe.
					ToSharedRef());

	Query.Hydrology =
		MakeShared<
			FVoxelHydrologyPlanner,
			ESPMode::ThreadSafe>(
				InConfig->Recipe.
					ToSharedRef(),
				Query.Terrain.
					ToSharedRef());

	Query.Cave =
		MakeShared<
			FVoxelCaveGenerator,
			ESPMode::ThreadSafe>(
				InConfig->Recipe.
					ToSharedRef());

	Query.Aquifer =
		MakeShared<
			FVoxelAquiferGenerator,
			ESPMode::ThreadSafe>(
				InConfig->Recipe.
					ToSharedRef());

	Query.Surface =
		MakeShared<
			FVoxelSurfaceGenerator,
			ESPMode::ThreadSafe>(
				InConfig->Recipe.
					ToSharedRef());

	Query.Ecology =
		MakeShared<FVoxelEcologyGenerator, ESPMode::ThreadSafe>(
			InConfig->Recipe.ToSharedRef());

	OutQuery =
		MoveTemp(Query);

	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::PrepareColumns(
	const FVoxelGenerationBounds& InBounds,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	if (!InBounds.IsValid())
	{
		OutError =
			TEXT("Voxel generation query bounds are invalid");

		return false;
	}

	if (InCancel &&
		InCancel->Load())
	{
		OutError =
			TEXT("Canceled");

		return false;
	}

	PreparedCaves.Reset();
	PreparedEcology.Reset();
	PreparedFeatures.Reset();
	PreparedStructures.Reset();
	PreparedBounds =
		InBounds;
	Cancel = InCancel;

	bColumnsPrepared = true;
	bSymbolsPrepared = false;

	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::Prepare(
	const FVoxelGenerationBounds& InBounds,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_QueryPrepare);

	if (!PrepareColumns(
		InBounds,
		OutError,
		InCancel))
	{
		return false;
	}

	const FVoxelGenerationSettings& Settings =
		Config->Recipe->Settings;

	const int32 PlanningMargin =
		FMath::Max(
			Settings.CaveSpacing,
			VoxelGenerationPlanTileSide);

	const FVoxelGenerationBounds PlanningBounds =
		InBounds.Expand(
			PlanningMargin);

	auto ColumnSampler =
		[this, InCancel](
			const FIntVector& InPosition,
			FVoxelColumnSample& OutColumn)
		{
			FString Error;
			return SampleEnvironmentColumn(
				InPosition.X,
				InPosition.Y,
				OutColumn,
				Error,
				InCancel);
		};

	auto SymbolSampler =
		[this, InCancel](
			const FIntVector& InPosition,
			uint32& OutSymbol)
		{
			FVoxelColumnSample Column;

			FString Error;
			if (!SampleEnvironmentColumn(
				InPosition.X,
				InPosition.Y,
				Column,
				Error,
				InCancel))
			{
				return false;
			}

			const int32 Density =
				Terrain->SampleDensityQ16(
					InPosition,
					{
						Column.SurfaceZ,
						Column.DensityHeight,
						Column.SlopePermille
					});

			OutSymbol =
				Density > 0
					? Config->Recipe->
						Palette.Stone
					: Config->Recipe->
						Palette.Air;

			return true;
		};

	const int32 MinTileX =
		VoxelGeneration::FloorDivide(
			PlanningBounds.Min.X,
			VoxelGenerationPlanTileSide);

	const int32 MaxTileX =
		VoxelGeneration::FloorDivide(
			PlanningBounds.Max.X - 1,
			VoxelGenerationPlanTileSide);

	const int32 MinTileY =
		VoxelGeneration::FloorDivide(
			PlanningBounds.Min.Y,
			VoxelGenerationPlanTileSide);

	const int32 MaxTileY =
		VoxelGeneration::FloorDivide(
			PlanningBounds.Max.Y - 1,
			VoxelGenerationPlanTileSide);

	for (int32 TileY = MinTileY;
		TileY <= MaxTileY;
		++TileY)
	{
		for (int32 TileX = MinTileX;
			TileX <= MaxTileX;
			++TileX)
		{
			if (InCancel &&
				InCancel->Load())
			{
				OutError =
					TEXT("Canceled");

				return false;
			}

			const FVoxelGenerationTileKey Key {
				FIntVector(
					TileX,
					TileY,
					0)
			};

			const FVoxelGenerationBounds TileBounds =
				MakeTileBounds(
					Key);

			FVoxelCavePlanPtr CavePlan;

			if (!Cache->GetOrBuildCave(
				Key,
				[
					this,
					TileBounds,
					&ColumnSampler,
					InCancel
				](
					FVoxelCavePlan& OutPlan,
					FString& BuildError)
				{
					return Cave->BuildPlan(
						TileBounds,
						ColumnSampler,
						OutPlan,
						BuildError,
						InCancel);
				},
				CavePlan,
				OutError,
				InCancel))
			{
				return false;
			}

			PreparedCaves.Add(
				CavePlan);

			FVoxelStructurePlanPtr StructurePlan;

			if (!Cache->GetOrBuildStructure(
				Key,
				[
					this,
					TileBounds,
					&ColumnSampler,
					InCancel
				](
					FVoxelStructurePlan& OutPlan,
					FString& BuildError)
				{
					TArray<FVoxelStructureInstance> Instances;
					FVoxelStructurePlanner Planner(
						Config->Recipe.ToSharedRef());

					if (!Planner.Plan(
						TileBounds,
						ColumnSampler,
						Instances,
						BuildError,
						InCancel))
					{
						return false;
					}

					OutPlan =
						FVoxelStructurePlan();

					OutPlan.Bounds =
						TileBounds;

					for (const FVoxelStructureInstance& Instance :
						Instances)
					{
						const int32 DefinitionIndex =
							Config->Recipe->
								FindStructure(
									Instance.
										DefinitionId);

						if (!Config->Recipe->
							Structures.
								IsValidIndex(
									DefinitionIndex))
						{
							continue;
						}

						const EVoxelGenerationStage Stage =
							Config->Recipe->
								Structures[
									DefinitionIndex].
									Stage;

						for (const FVoxelStructureClearVolume& Clear :
							Instance.ClearVolumes)
						{
							OutPlan.Clears.Add({
								Clear.Bounds,
								Stage,
								Instance.Id
							});
						}

						for (const FVoxelStructureCellWrite& Write :
							Instance.Writes)
						{
							OutPlan.Writes.Add({
								Write.Position,
								Write.Value,
								Stage,
								Instance.Id
							});
						}

						OutPlan.Details.Append(
							Instance.Details);
					}

					OutPlan.Finalize();

					BuildError.Reset();
					return true;
				},
				StructurePlan,
				OutError,
				InCancel))
			{
				return false;
			}

			PreparedStructures.Add(
				StructurePlan);

			FVoxelFeaturePlanPtr FeaturePlan;

			if (!Cache->GetOrBuildFeature(
				Key,
				[
					this,
					TileBounds,
					&ColumnSampler,
					&SymbolSampler,
					InCancel
				](
					FVoxelFeaturePlan& OutPlan,
					FString& BuildError)
				{
					TArray<FVoxelFeatureInstance> Instances;
					FVoxelFeaturePlanner Planner(
						Config->Recipe.ToSharedRef());

					if (!Planner.Plan(
						TileBounds,
						ColumnSampler,
						SymbolSampler,
						Instances,
						BuildError,
						InCancel))
					{
						return false;
					}

					OutPlan =
						FVoxelFeaturePlan();

					OutPlan.Bounds =
						TileBounds;

					for (const FVoxelFeatureInstance& Instance :
						Instances)
					{
						const int32 DefinitionIndex =
							Config->Recipe->
								FindFeature(
									Instance.
										DefinitionId);

						if (!Config->Recipe->
							Features.
								IsValidIndex(
									DefinitionIndex))
						{
							continue;
						}

						const EVoxelGenerationStage Stage =
							Config->Recipe->
								Features[
									DefinitionIndex].
									Stage;

						for (const FVoxelFeatureCellWrite& Write :
							Instance.Writes)
						{
							OutPlan.Writes.Add({
								Write.Position,
								Write.Value,
								Stage,
								Instance.Id
							});
						}
					}

					OutPlan.Finalize();

					BuildError.Reset();
					return true;
				},
				FeaturePlan,
				OutError,
				InCancel))
			{
				return false;
			}

			PreparedFeatures.Add(
				FeaturePlan);
		}
	}

	const int32 EcologyMargin = FMath::Max(
		Config->Recipe->Settings.Ecology.Tree.CrownRadius,
		Config->Recipe->Settings.Ecology.Grass.PatchRadius) + 2;
	const int32 EcologyMinX = VoxelGeneration::FloorDivide(InBounds.Min.X - EcologyMargin, EcologyTileSide);
	const int32 EcologyMaxX = VoxelGeneration::FloorDivide(InBounds.Max.X - 1 + EcologyMargin, EcologyTileSide);
	const int32 EcologyMinY = VoxelGeneration::FloorDivide(InBounds.Min.Y - EcologyMargin, EcologyTileSide);
	const int32 EcologyMaxY = VoxelGeneration::FloorDivide(InBounds.Max.Y - 1 + EcologyMargin, EcologyTileSide);
	for (int32 TileY = EcologyMinY; TileY <= EcologyMaxY; ++TileY)
	{
		for (int32 TileX = EcologyMinX; TileX <= EcologyMaxX; ++TileX)
		{
			const FVoxelEcologyTileKey Key { FIntPoint(TileX, TileY) };
			const FIntPoint MinXY = Key.Coordinate * EcologyTileSide;
			const FVoxelGenerationBounds Bounds {
				FIntVector(MinXY.X, MinXY.Y, Config->Recipe->Settings.MinZ),
				FIntVector(MinXY.X + EcologyTileSide, MinXY.Y + EcologyTileSide, Config->Recipe->Settings.MaxZ)
			};
			FVoxelEcologyPlanPtr Plan;
			if (!Cache->GetOrBuildEcology(
				Key,
				[this, Bounds, &ColumnSampler, &SymbolSampler, InCancel](FVoxelEcologyPlan& OutPlan, FString& BuildError)
				{
					return Ecology->BuildPlan(Bounds, ColumnSampler, SymbolSampler, OutPlan, BuildError, InCancel);
				},
				Plan,
				OutError,
				InCancel))
			{
				return false;
			}
			PreparedEcology.Add(Plan);
		}
	}

	// 在查询准备阶段剔除无关计划，避免对每个方块重复遍历整圈瓦片的空查询。
	PreparedCaves.RemoveAll([&InBounds](const FVoxelCavePlanPtr& Plan) { return !Plan || !Plan->AffectsBounds(InBounds); });
	PreparedEcology.RemoveAll([&InBounds](const FVoxelEcologyPlanPtr& Plan) { return !Plan || !Plan->AffectsBounds(InBounds); });
	PreparedStructures.RemoveAll([&InBounds](const FVoxelStructurePlanPtr& Plan) { return !Plan || !Plan->AffectsBounds(InBounds); });
	PreparedFeatures.RemoveAll([&InBounds](const FVoxelFeaturePlanPtr& Plan) { return !Plan || !Plan->AffectsBounds(InBounds); });

	bSymbolsPrepared = true;

	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::PrepareStructuresOnly(
	const FVoxelGenerationBounds& InBounds,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	if (!PrepareColumns(InBounds, OutError, InCancel))
	{
		return false;
	}

	auto ColumnSampler = [this, InCancel](
		const FIntVector& InPosition,
		FVoxelColumnSample& OutColumn)
	{
		FString Error;
		return SampleEnvironmentColumn(
			InPosition.X,
			InPosition.Y,
			OutColumn,
			Error,
			InCancel);
	};

	const FVoxelGenerationBounds PlanningBounds = InBounds.Expand(VoxelGenerationPlanTileSide);
	const int32 MinTileX = VoxelGeneration::FloorDivide(PlanningBounds.Min.X, VoxelGenerationPlanTileSide);
	const int32 MaxTileX = VoxelGeneration::FloorDivide(PlanningBounds.Max.X - 1, VoxelGenerationPlanTileSide);
	const int32 MinTileY = VoxelGeneration::FloorDivide(PlanningBounds.Min.Y, VoxelGenerationPlanTileSide);
	const int32 MaxTileY = VoxelGeneration::FloorDivide(PlanningBounds.Max.Y - 1, VoxelGenerationPlanTileSide);

	for (int32 TileY = MinTileY; TileY <= MaxTileY; ++TileY)
	{
		for (int32 TileX = MinTileX; TileX <= MaxTileX; ++TileX)
		{
			if (InCancel && InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}

			const FVoxelGenerationTileKey Key { FIntVector(TileX, TileY, 0) };
			const FVoxelGenerationBounds TileBounds = MakeTileBounds(Key);
			FVoxelStructurePlanPtr StructurePlan;
			if (!Cache->GetOrBuildStructure(
				Key,
				[this, TileBounds, &ColumnSampler, InCancel](FVoxelStructurePlan& OutPlan, FString& BuildError)
				{
					TArray<FVoxelStructureInstance> Instances;
					FVoxelStructurePlanner Planner(Config->Recipe.ToSharedRef());
					if (!Planner.Plan(TileBounds, ColumnSampler, Instances, BuildError, InCancel))
					{
						return false;
					}

					OutPlan = FVoxelStructurePlan();
					OutPlan.Bounds = TileBounds;
					for (const FVoxelStructureInstance& Instance : Instances)
					{
						const int32 DefinitionIndex = Config->Recipe->FindStructure(Instance.DefinitionId);
						if (!Config->Recipe->Structures.IsValidIndex(DefinitionIndex))
						{
							continue;
						}

						const EVoxelGenerationStage Stage = Config->Recipe->Structures[DefinitionIndex].Stage;
						for (const FVoxelStructureClearVolume& Clear : Instance.ClearVolumes)
						{
							OutPlan.Clears.Add({ Clear.Bounds, Stage, Instance.Id });
						}
						for (const FVoxelStructureCellWrite& Write : Instance.Writes)
						{
							OutPlan.Writes.Add({ Write.Position, Write.Value, Stage, Instance.Id });
						}
						OutPlan.Details.Append(Instance.Details);
					}

					OutPlan.Finalize();
					BuildError.Reset();
					return true;
				},
				StructurePlan,
				OutError,
				InCancel))
			{
				return false;
			}
			PreparedStructures.Add(StructurePlan);
		}
	}

	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::GetBaseColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& OutColumn,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}

	if (!bUseColumnCache)
	{
		if (!ComputeBaseColumn(
			InX,
			InY,
			OutColumn))
		{
			OutError = TEXT("Voxel base column generation failed");
			return false;
		}

		OutError.Reset();
		return true;
	}

	FVoxelBaseColumnEntryPtr Entry;
	if (!Cache->GetOrBuildBaseColumn(
		FIntPoint(InX, InY),
		[this, InX, InY](FVoxelBaseColumnEntry& OutEntry, FString& BuildError)
		{
			OutEntry.Position = FIntPoint(InX, InY);
			if (!ComputeBaseColumn(InX, InY, OutEntry.Column))
			{
				BuildError = TEXT("Voxel base column generation failed");
				return false;
			}
			BuildError.Reset();
			return true;
		},
		Entry,
		OutError,
		InCancel) || !Entry)
	{
		return false;
	}

	OutColumn = Entry->Column;
	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::ComputeBaseColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& OutColumn) const
{
	FVoxelColumnSample Column;

	Column.Climate =
		Climate->Sample(
			InX,
			InY);

	const FVoxelMacroTerrainSample TerrainSample =
		Terrain->SampleMacro(
			InX,
			InY);

	Column.SurfaceZ =
		TerrainSample.SurfaceZ;

	Column.DensityHeight =
		TerrainSample.DensityHeight;

	Column.SlopePermille =
		TerrainSample.SlopePermille;

	Column.Landform =
		TerrainSample.Landform;

	Column.BiomeIndex =
		Biome->Resolve(
			Column.Climate,
			TerrainSample);

	const int32 SeaLevel =
		Config->Recipe->
			Settings.SeaLevel;

	if (Column.SurfaceZ <=
		SeaLevel)
	{
		Column.SurfaceWaterZ =
			SeaLevel;

		Column.bOcean = true;
	}
	else
	{
		static const FIntPoint CoastNeighbors[] =
		{
			FIntPoint(-1, 0),
			FIntPoint(1, 0),
			FIntPoint(0, -1),
			FIntPoint(0, 1)
		};

		for (const FIntPoint& Offset :
			CoastNeighbors)
		{
			if (Terrain->SampleMacro(
				InX + Offset.X,
				InY + Offset.Y).
				SurfaceZ <=
				SeaLevel)
			{
				Column.bCoast = true;
				break;
			}
		}
	}

	OutColumn =
		Column;

	return true;
}

bool FVoxelGenerationQuery::ComputeNaturalColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& OutColumn,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_NaturalColumn);

	FVoxelColumnSample Column;
	if (!GetBaseColumn(InX, InY, Column, OutError, InCancel))
	{
		return false;
	}

	if (!ApplyHydrology(InX, InY, Column, OutError, InCancel))
	{
		return false;
	}

	Column.DensityHeight = Column.SurfaceZ;
	Surface->ResolveColumn(Column);
	OutColumn = Column;
	OutError.Reset();
	return true;
}

FVoxelHydrologyRegionKey FVoxelGenerationQuery::HydrologyRegionForVoxel(
	const int32 InX,
	const int32 InY) const
{
	const FVoxelGenerationSettings& Settings = Config->Recipe->Settings;
	const int32 CellSize = FMath::Max(1, Settings.HydrologyCellSize);
	const int32 RegionSide = FMath::Max(1, Settings.HydrologyRegionSide);
	return { FIntPoint(
		VoxelGeneration::FloorDivide(VoxelGeneration::FloorDivide(InX, CellSize), RegionSide),
		VoxelGeneration::FloorDivide(VoxelGeneration::FloorDivide(InY, CellSize), RegionSide)) };
}

bool FVoxelGenerationQuery::ApplyHydrology(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& InOutColumn,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	const FVoxelHydrologyRegionKey Key = HydrologyRegionForVoxel(InX, InY);
	FVoxelHydrologyPlanPtr Plan;
	if (!Cache->GetOrBuildHydrology(
		Key,
		[this, Key, InCancel](FVoxelHydrologyPlan& OutPlan, FString& BuildError)
		{
			return Hydrology->BuildPlan(Key, OutPlan, BuildError, InCancel);
		},
		Plan,
		OutError,
		InCancel) || !Plan)
	{
		return false;
	}

	FVoxelHydrologyInfluence Influence;
	if (!Plan->Sample(InX, InY, InOutColumn.SurfaceZ, Influence))
	{
		OutError.Reset();
		return true;
	}

	if (Influence.GroundOverrideZ != MIN_int32)
	{
		InOutColumn.SurfaceZ = Influence.GroundOverrideZ;
	}
	InOutColumn.SurfaceWaterZ = Influence.SurfaceWaterZ;
	InOutColumn.RiverDistanceCells = Influence.RiverDistanceCells;
	InOutColumn.BankDistanceCells = Influence.BankDistanceCells;
	InOutColumn.FloodplainStrengthQ15 = Influence.FloodplainStrengthQ15;
	InOutColumn.RiverId = Influence.RiverId;
	InOutColumn.bRiver = Influence.bRiver;
	InOutColumn.bLake = Influence.bLake;
	InOutColumn.bOcean = Influence.bOcean;
	InOutColumn.bCoast = Influence.bCoast;
	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::SampleEnvironmentColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& OutColumn,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (!bUseColumnCache)
	{
		return ComputeNaturalColumn(
			InX,
			InY,
			OutColumn,
			OutError,
			InCancel);
	}

	FVoxelNaturalColumnEntryPtr Entry;
	if (!Cache->GetOrBuildNaturalColumn(
		FIntPoint(InX, InY),
		[this, InX, InY, InCancel](FVoxelNaturalColumnEntry& OutEntry, FString& BuildError)
		{
			OutEntry.Position = FIntPoint(InX, InY);
			return ComputeNaturalColumn(InX, InY, OutEntry.Column, BuildError, InCancel);
		},
		Entry,
		OutError,
		InCancel) || !Entry)
	{
		return false;
	}
	OutColumn = Entry->Column;
	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::ResolveSurfaceCandidate(
	const int32 InX,
	const int32 InY,
	FVoxelSurfaceCandidate& OutCandidate,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	FVoxelColumnSample Column;
	if (!SampleEnvironmentColumn(InX, InY, Column, OutError, InCancel))
	{
		return false;
	}
	OutCandidate.XY = FIntPoint(InX, InY);
	OutCandidate.GroundZ = Column.SurfaceZ;
	OutCandidate.WaterZ = Column.SurfaceWaterZ;
	OutCandidate.SlopePermille = Column.SlopePermille;
	OutCandidate.BiomeIndex = Column.BiomeIndex;
	OutCandidate.bRiver = Column.bRiver;
	OutCandidate.bLake = Column.bLake;
	OutCandidate.bOcean = Column.bOcean;
	OutCandidate.bCoast = Column.bCoast;
	OutCandidate.bValid = OutCandidate.GroundZ >= Config->Recipe->Settings.MinZ &&
		OutCandidate.GroundZ < Config->Recipe->Settings.MaxZ - 2;
	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::IsInsidePreparedXY(
	const int32 InX,
	const int32 InY) const
{
	return
		InX >= PreparedBounds.Min.X &&
		InY >= PreparedBounds.Min.Y &&
		InX < PreparedBounds.Max.X &&
		InY < PreparedBounds.Max.Y;
}

TConstArrayView<FVoxelStructurePlanPtr>
FVoxelGenerationQuery::GetPreparedStructurePlans() const
{
	return PreparedStructures;
}

bool FVoxelGenerationQuery::SampleColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& OutColumn,
	FString& OutError) const
{
	if (!bColumnsPrepared)
	{
		OutError =
			TEXT("Voxel generation column query is not prepared");

		return false;
	}

	if (!SampleEnvironmentColumn(
		InX,
		InY,
		OutColumn,
		OutError,
		Cancel))
	{
		return false;
	}

	const FVoxelAquiferSample Sample =
		Aquifer->Sample(
			FIntVector(
				InX,
				InY,
				OutColumn.SurfaceZ),
			OutColumn,
			0);

	OutColumn.GroundWaterZ =
		Sample.SurfaceZ;

	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::SampleSymbol(
	const FIntVector& InPosition,
	uint32& OutValue,
	FString& OutError) const
{
	if (!bSymbolsPrepared ||
		!PreparedBounds.Contains(
			InPosition))
	{
		OutError =
			TEXT("Voxel generation position is outside prepared symbol bounds");

		return false;
	}

	FVoxelColumnSample Column;

	if (!SampleEnvironmentColumn(
		InPosition.X,
		InPosition.Y,
		Column,
		OutError,
		Cancel))
	{
		return false;
	}

	return ResolveSymbol(InPosition, Column, OutValue, OutError);
}

bool FVoxelGenerationQuery::SampleColumnSymbols(const FIntPoint& InColumn, const int32 InMinZ,
	const int32 InCount, TArray<uint32>& OutValues, FString& OutError) const
{
	if (!bSymbolsPrepared || InCount <= 0 ||
		!PreparedBounds.Contains(FIntVector(InColumn.X, InColumn.Y, InMinZ)) ||
		static_cast<int64>(InMinZ) + InCount > PreparedBounds.Max.Z)
	{
		OutError = TEXT("Voxel symbol column is outside prepared bounds");
		return false;
	}
	FVoxelColumnSample Column;
	if (!SampleEnvironmentColumn(InColumn.X, InColumn.Y, Column, OutError, Cancel))
	{
		return false;
	}
	TArray<uint32> Values;
	Values.SetNumUninitialized(InCount);
	for (int32 Index = 0; Index < InCount; ++Index)
	{
		if (Cancel && Cancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		if (!ResolveSymbol(FIntVector(InColumn.X, InColumn.Y, InMinZ + Index), Column, Values[Index], OutError))
		{
			return false;
		}
	}
	OutValues = MoveTemp(Values);
	return true;
}

bool FVoxelGenerationQuery::ResolveSymbol(const FIntVector& InPosition, FVoxelColumnSample Column,
	uint32& OutValue, FString& OutError) const
{
	uint32 Value =
		Config->Recipe->
			Palette.Air;

	static constexpr EVoxelGenerationStage Stages[] =
	{
		EVoxelGenerationStage::TerrainDensity,
		EVoxelGenerationStage::Aquifer,
		EVoxelGenerationStage::Surface,
		EVoxelGenerationStage::Carving,
		EVoxelGenerationStage::HydrologyRaster,
		EVoxelGenerationStage::BaseEcology,
		EVoxelGenerationStage::UndergroundStructures,
		EVoxelGenerationStage::SurfaceStructures,
		EVoxelGenerationStage::UndergroundOres,
		EVoxelGenerationStage::UndergroundDecoration,
		EVoxelGenerationStage::FluidFeatures,
		EVoxelGenerationStage::Vegetation,
		EVoxelGenerationStage::SurfaceDecoration,
		EVoxelGenerationStage::TopLayer
	};

	for (const EVoxelGenerationStage Stage :
		Stages)
	{
		if (!ApplyStage(
			Stage,
			InPosition,
			Column,
			Value,
			OutError))
		{
			return false;
		}
	}

	OutValue =
		Value;

	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::ApplyStage(
	const EVoxelGenerationStage InStage,
	const FIntVector& InPosition,
	FVoxelColumnSample& InOutColumn,
	uint32& InOutValue,
	FString& OutError) const
{
	const FVoxelGenerationPalette& Palette =
		Config->Recipe->Palette;

	switch (InStage)
	{
	case EVoxelGenerationStage::TerrainDensity:
	{
		const FVoxelMacroTerrainSample Macro {
			InOutColumn.SurfaceZ,
			InOutColumn.DensityHeight,
			InOutColumn.SlopePermille
		};

		InOutValue =
			Terrain->SampleDensityQ16(
				InPosition,
				Macro) > 0
				? Palette.Stone
				: Palette.Air;

		break;
	}

	case EVoxelGenerationStage::Aquifer:
		if ((InOutValue & 0xffffu) ==
			Palette.Air)
		{
			const FVoxelAquiferSample Sample =
				Aquifer->Sample(
					InPosition,
					InOutColumn,
					-1);

			if (Sample.Fluid ==
				EVoxelFluidKind::Water)
			{
				InOutValue =
					Palette.Water;
			}
			else if (
				Sample.Fluid ==
				EVoxelFluidKind::Lava)
			{
				InOutValue =
					Palette.Lava;
			}
		}
		break;

	case EVoxelGenerationStage::Surface:
		if ((InOutValue & 0xffffu) !=
				Palette.Air &&
			InPosition.Z <=
				InOutColumn.SurfaceZ)
		{
			InOutValue =
				Surface->ResolveSymbol(
					InPosition,
					InOutColumn,
					InOutColumn.SurfaceZ -
						InPosition.Z);
		}
		break;

	case EVoxelGenerationStage::Carving:
		for (const FVoxelCavePlanPtr& Plan :
			PreparedCaves)
		{
			if (Plan &&
				Plan->Carves(
					InPosition))
			{
				InOutValue =
					Palette.Air;

				break;
			}
		}
		break;

	case EVoxelGenerationStage::HydrologyRaster:
		if (InOutColumn.SurfaceWaterZ !=
				MIN_int32 &&
			InPosition.Z >
				InOutColumn.SurfaceZ &&
			InPosition.Z <=
				InOutColumn.SurfaceWaterZ)
		{
			InOutValue =
				Palette.Water;
		}
		break;

	case EVoxelGenerationStage::BaseEcology:
		if (static_cast<uint16>(InOutValue & 0xffffu) != Palette.Air)
		{
			break;
		}
		for (const FVoxelCavePlanPtr& Plan : PreparedCaves)
		{
			if (Plan && Plan->Carves(InPosition))
			{
				return true;
			}
		}
		for (const FVoxelEcologyPlanPtr& Plan : PreparedEcology)
		{
			uint32 PlannedValue = 0;
			if (Plan && Plan->Sample(InPosition, PlannedValue))
			{
				InOutValue = PlannedValue;
				break;
			}
		}
		break;

	case EVoxelGenerationStage::UndergroundStructures:
	case EVoxelGenerationStage::SurfaceStructures:
		for (const FVoxelStructurePlanPtr& Plan :
			PreparedStructures)
		{
			if (!Plan)
			{
				continue;
			}

			if (Plan->IsCleared(
				InPosition,
				InStage))
			{
				InOutValue =
					Palette.Air;
			}

			uint32 PlannedValue = 0;

			if (Plan->Sample(
				InPosition,
				InStage,
				PlannedValue))
			{
				InOutValue =
					PlannedValue;
			}
		}
		break;

	default:
		for (const FVoxelFeaturePlanPtr& Plan :
			PreparedFeatures)
		{
			uint32 PlannedValue = 0;

			if (!Plan ||
				!Plan->Sample(
					InPosition,
					InStage,
					PlannedValue))
			{
				continue;
			}

			if ((InStage == EVoxelGenerationStage::Vegetation ||
				 InStage == EVoxelGenerationStage::SurfaceDecoration) &&
				static_cast<uint16>(InOutValue & 0xffffu) != Palette.Air)
			{
				continue;
			}

			InOutValue = PlannedValue;
		}
		break;
	}

	OutError.Reset();
	return true;
}
