#include "Voxel/Generation/VoxelGenerationQuery.h"

#include "Voxel/Generation/Aquifer/VoxelAquiferGenerator.h"
#include "Voxel/Generation/Biome/VoxelBiomeGenerator.h"
#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/Hydrology/VoxelHydrology.h"
#include "Voxel/Generation/Surface/VoxelSurfaceGenerator.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/VoxelFeature.h"
#include "Voxel/Generation/VoxelStructure.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	constexpr int32 VoxelGenerationPlanTileSide =
		256;

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

	void MergeHydrology(
		const FVoxelHydrologyInfluence& InCandidate,
		FVoxelHydrologyInfluence& InOutResult)
	{
		if (InCandidate.bOcean ||
			InCandidate.bLake)
		{
			if ((!InOutResult.bOcean &&
				 !InOutResult.bLake) ||
				InCandidate.SurfaceWaterZ >
					InOutResult.SurfaceWaterZ)
			{
				InOutResult =
					InCandidate;
			}

			return;
		}

		if (!InOutResult.bOcean &&
			!InOutResult.bLake)
		{
			if (InCandidate.GroundOverrideZ !=
					MIN_int32 &&
				(InOutResult.GroundOverrideZ ==
						MIN_int32 ||
				 InCandidate.GroundOverrideZ <
						InOutResult.GroundOverrideZ))
			{
				InOutResult.GroundOverrideZ =
					InCandidate.GroundOverrideZ;
			}

			if (InCandidate.SurfaceWaterZ >
				InOutResult.SurfaceWaterZ)
			{
				InOutResult.SurfaceWaterZ =
					InCandidate.SurfaceWaterZ;
			}

			InOutResult.bRiver |=
				InCandidate.bRiver;
		}

		InOutResult.bCoast |=
			InCandidate.bCoast;
	}
}

bool FVoxelGenerationQuery::Create(
	TSharedRef<
		const FVoxelGenerationRuntimeConfig,
		ESPMode::ThreadSafe> InConfig,
	TSharedRef<
		FVoxelGenerationPlanCache,
		ESPMode::ThreadSafe> InCache,
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

	Query.Config =
		InConfig;

	Query.Cache =
		InCache;

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
			FVoxelHydrologyGenerator,
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
					ToSharedRef(),
				Query.Terrain.
					ToSharedRef(),
				Query.Hydrology.
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

	PreparedHydrology.Reset();
	PreparedCaves.Reset();
	PreparedFeatures.Reset();
	PreparedStructures.Reset();
	PreparedColumnCache.Reset();

	PreparedBounds =
		InBounds;

	bColumnsPrepared = false;
	bSymbolsPrepared = false;

	if (!PrepareHydrology(
		InBounds,
		OutError,
		InCancel))
	{
		return false;
	}

	bColumnsPrepared = true;

	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::Prepare(
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

	PreparedHydrology.Reset();
	PreparedCaves.Reset();
	PreparedFeatures.Reset();
	PreparedStructures.Reset();
	PreparedColumnCache.Reset();

	PreparedBounds =
		InBounds;

	bColumnsPrepared = false;
	bSymbolsPrepared = false;

	const FVoxelGenerationSettings& Settings =
		Config->Recipe->Settings;

	const int32 PlanningMargin =
		FMath::Max(
			Settings.CaveSpacing,
			VoxelGenerationPlanTileSide);

	const FVoxelGenerationBounds
		PlanningBounds =
			InBounds.Expand(
				PlanningMargin);

	/*
	 * Structure / Feature Planner 会在 Planning Tile
	 * 上采 Column，因此 Hydrology 必须覆盖 PlanningBounds，
	 * 不能只覆盖最终 16³ Section。
	 */
	if (!PrepareHydrology(
		PlanningBounds,
		OutError,
		InCancel))
	{
		return false;
	}

	bColumnsPrepared = true;

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

			const FVoxelGenerationBounds
				TileBounds =
					MakeTileBounds(Key);

			FVoxelCavePlanPtr CavePlan;

			if (!Cache->GetOrBuildCave(
				Key,
				[
					this,
					TileBounds,
					InCancel
				](
					FVoxelCavePlan& OutPlan,
					FString& BuildError)
				{
					return Cave->BuildPlan(
						TileBounds,
						OutPlan,
						BuildError,
						InCancel);
				},
				CavePlan,
				OutError))
			{
				return false;
			}

			PreparedCaves.Add(
				CavePlan);

			FVoxelStructurePlanPtr
				StructurePlan;

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
					TArray<
						FVoxelStructureInstance>
						Instances;

					FVoxelStructurePlanner Planner(
						Config->Recipe.
							ToSharedRef());

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

					for (const
						FVoxelStructureInstance&
							Instance :
						Instances)
					{
						const int32
							DefinitionIndex =
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

						const
							EVoxelGenerationStage
								Stage =
									Config->Recipe->
										Structures[
											DefinitionIndex].
										Stage;

						for (const
							FVoxelStructureClearVolume&
								Clear :
							Instance.ClearVolumes)
						{
							OutPlan.Clears.Add({
								Clear.Bounds,
								Stage,
								Instance.Id
							});
						}

						for (const
							FVoxelStructureCellWrite&
								Write :
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
				OutError))
			{
				return false;
			}

			PreparedStructures.Add(
				StructurePlan);

			FVoxelFeaturePlanPtr
				FeaturePlan;

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
					TArray<
						FVoxelFeatureInstance>
						Instances;

					FVoxelFeaturePlanner Planner(
						Config->Recipe.
							ToSharedRef());

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

					for (const
						FVoxelFeatureInstance&
							Instance :
						Instances)
					{
						const int32
							DefinitionIndex =
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

						const
							EVoxelGenerationStage
								Stage =
									Config->Recipe->
										Features[
											DefinitionIndex].
										Stage;

						for (const
							FVoxelFeatureCellWrite&
								Write :
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
				OutError))
			{
				return false;
			}

			PreparedFeatures.Add(
				FeaturePlan);
		}
	}

	PreparedHydrology.Sort(
		[](
			const FVoxelHydrologyPlanPtr& InA,
			const FVoxelHydrologyPlanPtr& InB)
		{
			return InA->Key <
				InB->Key;
		});

	bSymbolsPrepared = true;

	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::PrepareHydrology(
	const FVoxelGenerationBounds& InBounds,
	FString& OutError,
	const TAtomic<bool>* InCancel)
{
	const FVoxelGenerationSettings& Settings =
		Config->Recipe->Settings;

	const int32 CellSize =
		FMath::Max(
			1,
			Settings.HydrologyCellSize);

	const int32 RegionSide =
		FMath::Max(
			1,
			Settings.HydrologyRegionSide);

	const int32 Halo =
		FMath::Max(
			0,
			Settings.HydrologyHaloCells);

	const int32 MinHydrologyX =
		VoxelGeneration::FloorDivide(
			InBounds.Min.X,
			CellSize);

	const int32 MaxHydrologyX =
		VoxelGeneration::FloorDivide(
			InBounds.Max.X - 1,
			CellSize);

	const int32 MinHydrologyY =
		VoxelGeneration::FloorDivide(
			InBounds.Min.Y,
			CellSize);

	const int32 MaxHydrologyY =
		VoxelGeneration::FloorDivide(
			InBounds.Max.Y - 1,
			CellSize);

	/*
	 * 一个 Plan 的 Grid = Core ± Halo。
	 * 因此只有 Core 与目标 Hydrology Cell Range
	 * 在加减 Halo 后相交的 Region 才需要准备。
	 *
	 * 默认值下，普通 Section 从固定 5×5
	 * 降到通常 2×2。
	 */
	const int32 MinRegionX =
		VoxelGeneration::FloorDivide(
			MinHydrologyX - Halo,
			RegionSide);

	const int32 MaxRegionX =
		VoxelGeneration::FloorDivide(
			MaxHydrologyX + Halo,
			RegionSide);

	const int32 MinRegionY =
		VoxelGeneration::FloorDivide(
			MinHydrologyY - Halo,
			RegionSide);

	const int32 MaxRegionY =
		VoxelGeneration::FloorDivide(
			MaxHydrologyY + Halo,
			RegionSide);

	for (int32 RegionY = MinRegionY;
		RegionY <= MaxRegionY;
		++RegionY)
	{
		for (int32 RegionX = MinRegionX;
			RegionX <= MaxRegionX;
			++RegionX)
		{
			if (InCancel &&
				InCancel->Load())
			{
				OutError =
					TEXT("Canceled");

				return false;
			}

			const FVoxelHydrologyRegionKey Key {
				FIntPoint(
					RegionX,
					RegionY)
			};

			FVoxelHydrologyPlanPtr Plan;

			if (!Cache->GetOrBuildHydrology(
				Key,
				[
					this,
					Key,
					InCancel
				](
					FVoxelHydrologyPlan& OutPlan,
					FString& BuildError)
				{
					return Hydrology->BuildPlan(
						Key,
						OutPlan,
						BuildError,
						InCancel);
				},
				Plan,
				OutError))
			{
				return false;
			}

			PreparedHydrology.Add(
				Plan);
		}
	}

	return true;
}

bool FVoxelGenerationQuery::SampleBaseColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& OutColumn) const
{
	if (bColumnsPrepared &&
		IsInsidePreparedXY(
			InX,
			InY))
	{
		const FIntPoint Key(
			InX,
			InY);

		if (const FVoxelColumnSample* Cached =
			PreparedColumnCache.Find(Key))
		{
			OutColumn = *Cached;
			return true;
		}

		FVoxelColumnSample Column;

		if (!ComputeBaseColumn(
			InX,
			InY,
			Column))
		{
			return false;
		}

		PreparedColumnCache.Add(
			Key,
			Column);

		OutColumn =
			Column;

		return true;
	}

	return ComputeBaseColumn(
		InX,
		InY,
		OutColumn);
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

	const FVoxelMacroTerrainSample
		TerrainSample =
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

	FVoxelHydrologyInfluence Influence;

	for (const FVoxelHydrologyPlanPtr& Plan :
		PreparedHydrology)
	{
		FVoxelHydrologyInfluence Candidate;

		if (Plan &&
			Plan->Sample(
				InX,
				InY,
				Column.SurfaceZ,
				Candidate))
		{
			MergeHydrology(
				Candidate,
				Influence);
		}
	}

	if (Influence.GroundOverrideZ !=
		MIN_int32)
	{
		Column.SurfaceZ =
			Influence.GroundOverrideZ;
	}

	Column.SurfaceWaterZ =
		Influence.SurfaceWaterZ;

	Column.bRiver =
		Influence.bRiver;

	Column.bLake =
		Influence.bLake;

	Column.bOcean =
		Influence.bOcean;

	Column.bCoast =
		Influence.bCoast;

	Surface->ResolveColumn(
		Column);

	OutColumn =
		Column;

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

	static constexpr
		EVoxelGenerationStage Stages[] =
	{
		EVoxelGenerationStage::TerrainDensity,
		EVoxelGenerationStage::Aquifer,
		EVoxelGenerationStage::Surface,
		EVoxelGenerationStage::Carving,
		EVoxelGenerationStage::HydrologyRaster,
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
				!Plan->ProtectsFloor(
					InPosition) &&
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

			if (Plan &&
				Plan->Sample(
					InPosition,
					InStage,
					PlannedValue))
			{
				InOutValue =
					PlannedValue;
			}
		}
		break;
	}

	OutError.Reset();
	return true;
}
