#include "Voxel/Generation/VoxelGenerationQuery.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/Aquifer/VoxelAquiferGenerator.h"
#include "Voxel/Generation/Biome/VoxelBiomeGenerator.h"
#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"
#include "Voxel/Generation/Hydrology/VoxelLakeGenerator.h"
#include "Voxel/Generation/Hydrology/VoxelRiverGenerator.h"
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
	FString& OutError)
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

	Query.River =
		MakeShared<
			FVoxelRiverGenerator,
			ESPMode::ThreadSafe>(
				InConfig->Recipe.
					ToSharedRef(),
				Query.Terrain.
					ToSharedRef());

	Query.Lake =
		MakeShared<
			FVoxelLakeGenerator,
			ESPMode::ThreadSafe>(
				InConfig->Recipe.
					ToSharedRef(),
				Query.Terrain.
					ToSharedRef(),
				Query.River.
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
		[this](
			const FIntVector& InPosition,
			FVoxelColumnSample& OutColumn)
		{
			return SampleBaseColumn(
				InPosition.X,
				InPosition.Y,
				OutColumn);
		};

	auto SymbolSampler =
		[this](
			const FIntVector& InPosition,
			uint32& OutSymbol)
		{
			FVoxelColumnSample Column;

			if (!SampleBaseColumn(
				InPosition.X,
				InPosition.Y,
				Column))
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

			FVoxelEcologyPlanPtr EcologyPlan;
			if (!Cache->GetOrBuildEcology(
				Key,
				[this, TileBounds, &ColumnSampler, &SymbolSampler, InCancel](
					FVoxelEcologyPlan& OutPlan,
					FString& BuildError)
				{
					return Ecology->BuildPlan(
						TileBounds,
						ColumnSampler,
						SymbolSampler,
						OutPlan,
						BuildError,
						InCancel);
				},
				EcologyPlan,
				OutError,
				InCancel))
			{
				return false;
			}
			PreparedEcology.Add(EcologyPlan);

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

	auto ColumnSampler = [this](
		const FIntVector& InPosition,
		FVoxelColumnSample& OutColumn)
	{
		return SampleBaseColumn(InPosition.X, InPosition.Y, OutColumn);
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

bool FVoxelGenerationQuery::SampleBaseColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& OutColumn) const
{
	if (Cancel && Cancel->Load())
	{
		return false;
	}

	const FIntPoint Position(InX, InY);
	FVoxelNaturalColumnEntryPtr Entry;
	FString Error;
	const bool bBuilt = Cache->GetOrBuildNaturalColumn(
		Position,
		[this, InX, InY](FVoxelNaturalColumnEntry& OutEntry, FString& BuildError)
		{
			OutEntry.Position = FIntPoint(InX, InY);
			return ComputeNaturalColumn(InX, InY, OutEntry.Column, BuildError);
		},
		Entry,
		Error,
		Cancel);

	if (!bBuilt || !Entry)
	{
		return false;
	}

	OutColumn = Entry->Column;
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
	FString& OutError) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_NaturalColumn);

	if (Cancel && Cancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}

	FVoxelBaseColumnEntryPtr BaseEntry;
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
		BaseEntry,
		OutError,
		Cancel) || !BaseEntry)
	{
		return false;
	}

	FVoxelColumnSample Column = BaseEntry->Column;
	if (!Column.bOcean)
	{
		const FVoxelNaturalTileKey RiverKey = VoxelNaturalTileKeyFromCell(InX, InY);
		FVoxelRiverFieldTilePtr RiverTile;
		auto BaseColumnLookup = [this](
			const int32 X,
			const int32 Y,
			FVoxelColumnSample& OutBase,
			FString& Error)
		{
			FVoxelBaseColumnEntryPtr Entry;
			if (!Cache->GetOrBuildBaseColumn(
				FIntPoint(X, Y),
				[this, X, Y](FVoxelBaseColumnEntry& OutEntry, FString& BuildError)
				{
					OutEntry.Position = FIntPoint(X, Y);
					if (!ComputeBaseColumn(X, Y, OutEntry.Column))
					{
						BuildError = TEXT("Voxel base column generation failed");
						return false;
					}
					BuildError.Reset();
					return true;
			},
			Entry,
			Error,
			Cancel) || !Entry)
			{
				return false;
			}
			OutBase = Entry->Column;
			return true;
		};

		if (!Cache->GetOrBuildRiverField(
			RiverKey,
			[this, RiverKey, &BaseColumnLookup](FVoxelRiverFieldTile& OutTile, FString& BuildError)
			{
				return River->BuildFieldTile(RiverKey, BaseColumnLookup, OutTile, BuildError, Cancel);
			},
			RiverTile,
			OutError,
			Cancel) || !RiverTile)
		{
			return false;
		}

		FVoxelRiverFieldSample RiverSample;
		if (!RiverTile->Sample(InX, InY, RiverSample))
		{
			OutError = TEXT("Voxel river field tile did not contain the requested column");
			return false;
		}
		River->ApplyToColumn(InX, InY, RiverSample, Column);

		const int32 AnchorX = VoxelGeneration::FloorDivide(InX, FVoxelLakeGenerator::AnchorSide);
		const int32 AnchorY = VoxelGeneration::FloorDivide(InY, FVoxelLakeGenerator::AnchorSide);
		for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
		{
			for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
			{
				const FVoxelLakeAnchorKey LakeKey { FIntPoint(AnchorX + OffsetX, AnchorY + OffsetY) };
				FVoxelLakeAnchorPlanPtr LakePlan;
				if (!Cache->GetOrBuildLake(
					LakeKey,
					[this, LakeKey](FVoxelLakeAnchorPlan& OutPlan, FString& BuildError)
					{
						return Lake->BuildPlan(LakeKey, OutPlan, BuildError, Cancel);
					},
					LakePlan,
					OutError,
					Cancel))
				{
					return false;
				}
				if (LakePlan)
				{
					Lake->ApplyFeature(*LakePlan, InX, InY, Column);
				}
			}
		}
	}

	Column.DensityHeight = Column.SurfaceZ;
	Surface->ResolveColumn(Column);
	OutColumn = Column;
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

	if (!SampleBaseColumn(
		InX,
		InY,
		OutColumn))
	{
		OutError =
			TEXT("Voxel generation column sampling failed");

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

	if (!SampleBaseColumn(
		InPosition.X,
		InPosition.Y,
		Column))
	{
		OutError =
			TEXT("Voxel generation base column sampling failed");

		return false;
	}

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
		for (const FVoxelEcologyPlanPtr& Plan : PreparedEcology)
		{
			uint32 PlannedValue = 0;
			if (Plan && Plan->Sample(InPosition, PlannedValue))
			{
				InOutValue = PlannedValue;
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
