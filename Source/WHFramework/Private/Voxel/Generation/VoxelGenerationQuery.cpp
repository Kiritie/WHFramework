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
	constexpr int32 VoxelGenerationPlanTileSide = 256;

	FVoxelGenerationBounds MakeTileBounds(const FVoxelGenerationTileKey& Key)
	{
		const FIntVector Min = Key.Coordinate * VoxelGenerationPlanTileSide;
		return {Min, Min + FIntVector(VoxelGenerationPlanTileSide)};
	}

	void MergeHydrology(const FVoxelHydrologyInfluence& Candidate, FVoxelHydrologyInfluence& Result)
	{
		if (Candidate.bOcean || Candidate.bLake)
		{
			if (!Result.bOcean && !Result.bLake || Candidate.SurfaceWaterZ > Result.SurfaceWaterZ)
			{
				Result = Candidate;
			}
			return;
		}
		if (!Result.bOcean && !Result.bLake)
		{
			if (Candidate.GroundOverrideZ != MIN_int32 &&
				(Result.GroundOverrideZ == MIN_int32 || Candidate.GroundOverrideZ < Result.GroundOverrideZ))
			{
				Result.GroundOverrideZ = Candidate.GroundOverrideZ;
			}
			if (Candidate.SurfaceWaterZ > Result.SurfaceWaterZ)
			{
				Result.SurfaceWaterZ = Candidate.SurfaceWaterZ;
			}
			Result.bRiver |= Candidate.bRiver;
		}
		Result.bCoast |= Candidate.bCoast;
	}
}

bool FVoxelGenerationQuery::Create(TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
	FVoxelGenerationQuery& OutQuery, FString& OutError)
{
	if (!InConfig->IsValid() || !InConfig->Recipe)
	{
		OutError = TEXT("Voxel generation query received an invalid runtime config");
		return false;
	}

	FVoxelGenerationQuery Query;
	Query.Config = InConfig;
	Query.Cache = InCache;
	Query.Climate = MakeShared<FVoxelClimateGenerator, ESPMode::ThreadSafe>(InConfig->Recipe.ToSharedRef());
	Query.Terrain = MakeShared<FVoxelTerrainGenerator, ESPMode::ThreadSafe>(InConfig->Recipe.ToSharedRef(), Query.Climate.ToSharedRef());
	Query.Biome = MakeShared<FVoxelBiomeGenerator, ESPMode::ThreadSafe>(InConfig->Recipe.ToSharedRef());
	Query.Hydrology = MakeShared<FVoxelHydrologyGenerator, ESPMode::ThreadSafe>(InConfig->Recipe.ToSharedRef(), Query.Terrain.ToSharedRef());
	Query.Cave = MakeShared<FVoxelCaveGenerator, ESPMode::ThreadSafe>(InConfig->Recipe.ToSharedRef(), Query.Terrain.ToSharedRef(), Query.Hydrology.ToSharedRef());
	Query.Aquifer = MakeShared<FVoxelAquiferGenerator, ESPMode::ThreadSafe>(InConfig->Recipe.ToSharedRef());
	Query.Surface = MakeShared<FVoxelSurfaceGenerator, ESPMode::ThreadSafe>(InConfig->Recipe.ToSharedRef());
	OutQuery = MoveTemp(Query);
	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::Prepare(const FVoxelGenerationBounds& InBounds, FString& OutError, const TAtomic<bool>* InCancel)
{
	if (!InBounds.IsValid())
	{
		OutError = TEXT("Voxel generation query bounds are invalid");
		return false;
	}
	bPrepared = false;
	PreparedHydrology.Reset();
	PreparedCaves.Reset();
	PreparedFeatures.Reset();
	PreparedStructures.Reset();

	const FVoxelGenerationSettings& Settings = Config->Recipe->Settings;
	const int32 OwnerMargin = 1 + VoxelGeneration::CeilDividePositive(Settings.HydrologyHaloCells, Settings.HydrologyRegionSide);
	const int32 HydroVoxelSide = Settings.HydrologyCellSize * Settings.HydrologyRegionSide;
	const int32 MinRegionX = VoxelGeneration::FloorDivide(InBounds.Min.X, HydroVoxelSide) - OwnerMargin;
	const int32 MaxRegionX = VoxelGeneration::FloorDivide(InBounds.Max.X - 1, HydroVoxelSide) + OwnerMargin;
	const int32 MinRegionY = VoxelGeneration::FloorDivide(InBounds.Min.Y, HydroVoxelSide) - OwnerMargin;
	const int32 MaxRegionY = VoxelGeneration::FloorDivide(InBounds.Max.Y - 1, HydroVoxelSide) + OwnerMargin;
	for (int32 RegionY = MinRegionY; RegionY <= MaxRegionY; ++RegionY)
	{
		for (int32 RegionX = MinRegionX; RegionX <= MaxRegionX; ++RegionX)
		{
			const FVoxelHydrologyRegionKey Key{FIntPoint(RegionX, RegionY)};
			FVoxelHydrologyPlanPtr Plan;
			if (!Cache->FindHydrology(Key, Plan))
			{
				FVoxelHydrologyPlan LocalPlan;
				if (!Hydrology->BuildPlan(Key, LocalPlan, OutError, InCancel)) return false;
				Plan = MakeShared<const FVoxelHydrologyPlan, ESPMode::ThreadSafe>(MoveTemp(LocalPlan));
				Cache->StoreHydrology(Key, Plan);
			}
			PreparedHydrology.Add(Plan);
		}
	}

	const int32 Margin = FMath::Max(Settings.CaveSpacing, VoxelGenerationPlanTileSide);
	const FVoxelGenerationBounds PlanningBounds = InBounds.Expand(Margin);
	const int32 MinTileX = VoxelGeneration::FloorDivide(PlanningBounds.Min.X, VoxelGenerationPlanTileSide);
	const int32 MaxTileX = VoxelGeneration::FloorDivide(PlanningBounds.Max.X - 1, VoxelGenerationPlanTileSide);
	const int32 MinTileY = VoxelGeneration::FloorDivide(PlanningBounds.Min.Y, VoxelGenerationPlanTileSide);
	const int32 MaxTileY = VoxelGeneration::FloorDivide(PlanningBounds.Max.Y - 1, VoxelGenerationPlanTileSide);

	auto ColumnSampler = [this](const FIntVector& Position, FVoxelColumnSample& OutColumn)
	{
		return SampleBaseColumn(Position.X, Position.Y, OutColumn);
	};
	auto SymbolSampler = [this](const FIntVector& Position, uint32& OutSymbol)
	{
		FVoxelColumnSample Column;
		if (!SampleBaseColumn(Position.X, Position.Y, Column)) return false;
		const int32 Density = Terrain->SampleDensityQ16(Position, {Column.SurfaceZ, Column.DensityHeight, Column.SlopePermille});
		OutSymbol = Density > 0 ? Config->Recipe->Palette.Stone : Config->Recipe->Palette.Air;
		return true;
	};

	for (int32 TileY = MinTileY; TileY <= MaxTileY; ++TileY)
	{
		for (int32 TileX = MinTileX; TileX <= MaxTileX; ++TileX)
		{
			const FVoxelGenerationTileKey Key{FIntVector(TileX, TileY, 0)};
			const FVoxelGenerationBounds TileBounds = MakeTileBounds(Key);
			FVoxelCavePlanPtr CavePlan;
			if (!Cache->FindCave(Key, CavePlan))
			{
				FVoxelCavePlan LocalPlan;
				if (!Cave->BuildPlan(TileBounds, LocalPlan, OutError, InCancel)) return false;
				CavePlan = MakeShared<const FVoxelCavePlan, ESPMode::ThreadSafe>(MoveTemp(LocalPlan));
				Cache->StoreCave(Key, CavePlan);
			}
			PreparedCaves.Add(CavePlan);

			FVoxelStructurePlanPtr StructurePlan;
			if (!Cache->FindStructure(Key, StructurePlan))
			{
				TArray<FVoxelStructureInstance> Instances;
				FVoxelStructurePlanner Planner(Config->Recipe.ToSharedRef());
				if (!Planner.Plan(TileBounds, ColumnSampler, Instances, OutError, InCancel)) return false;
				FVoxelStructurePlan LocalPlan;
				LocalPlan.Bounds = TileBounds;
				for (const FVoxelStructureInstance& Instance : Instances)
				{
					const int32 DefinitionIndex = Config->Recipe->FindStructure(Instance.DefinitionId);
					if (!Config->Recipe->Structures.IsValidIndex(DefinitionIndex)) continue;
					const EVoxelGenerationStage Stage = Config->Recipe->Structures[DefinitionIndex].Stage;
					for (const FVoxelStructureClearVolume& Clear : Instance.ClearVolumes) LocalPlan.Clears.Add({Clear.Bounds, Stage, Instance.Id});
					for (const FVoxelStructureCellWrite& Write : Instance.Writes) LocalPlan.Writes.Add({Write.Position, Write.Value, Stage, Instance.Id});
					LocalPlan.Details.Append(Instance.Details);
				}
				StructurePlan = MakeShared<const FVoxelStructurePlan, ESPMode::ThreadSafe>(MoveTemp(LocalPlan));
				Cache->StoreStructure(Key, StructurePlan);
			}
			PreparedStructures.Add(StructurePlan);

			FVoxelFeaturePlanPtr FeaturePlan;
			if (!Cache->FindFeature(Key, FeaturePlan))
			{
				TArray<FVoxelFeatureInstance> Instances;
				FVoxelFeaturePlanner Planner(Config->Recipe.ToSharedRef());
				if (!Planner.Plan(TileBounds, ColumnSampler, SymbolSampler, Instances, OutError, InCancel)) return false;
				FVoxelFeaturePlan LocalPlan;
				LocalPlan.Bounds = TileBounds;
				for (const FVoxelFeatureInstance& Instance : Instances)
				{
					const int32 DefinitionIndex = Config->Recipe->FindFeature(Instance.DefinitionId);
					if (!Config->Recipe->Features.IsValidIndex(DefinitionIndex)) continue;
					const EVoxelGenerationStage Stage = Config->Recipe->Features[DefinitionIndex].Stage;
					for (const FVoxelFeatureCellWrite& Write : Instance.Writes) LocalPlan.Writes.Add({Write.Position, Write.Value, Stage, Instance.Id});
				}
				FeaturePlan = MakeShared<const FVoxelFeaturePlan, ESPMode::ThreadSafe>(MoveTemp(LocalPlan));
				Cache->StoreFeature(Key, FeaturePlan);
			}
			PreparedFeatures.Add(FeaturePlan);
		}
	}

	PreparedHydrology.Sort([](const FVoxelHydrologyPlanPtr& A, const FVoxelHydrologyPlanPtr& B) { return A->Key < B->Key; });
	PreparedCaves.Sort([](const FVoxelCavePlanPtr& A, const FVoxelCavePlanPtr& B) { return A->Routes.Num() < B->Routes.Num(); });
	PreparedBounds = InBounds;
	bPrepared = true;
	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::SampleBaseColumn(int32 InX, int32 InY, FVoxelColumnSample& OutColumn) const
{
	FVoxelColumnSample Column;
	Column.Climate = Climate->Sample(InX, InY);
	const FVoxelMacroTerrainSample TerrainSample = Terrain->SampleMacro(InX, InY);
	Column.SurfaceZ = TerrainSample.SurfaceZ;
	Column.DensityHeight = TerrainSample.DensityHeight;
	Column.SlopePermille = TerrainSample.SlopePermille;
	Column.BiomeIndex = Biome->Resolve(Column.Climate, TerrainSample);
	FVoxelHydrologyInfluence Influence;
	for (const FVoxelHydrologyPlanPtr& Plan : PreparedHydrology)
	{
		FVoxelHydrologyInfluence Candidate;
		if (Plan && Plan->Sample(InX, InY, Column.SurfaceZ, Candidate)) MergeHydrology(Candidate, Influence);
	}
	if (Influence.GroundOverrideZ != MIN_int32) Column.SurfaceZ = Influence.GroundOverrideZ;
	Column.SurfaceWaterZ = Influence.SurfaceWaterZ;
	Column.bRiver = Influence.bRiver;
	Column.bLake = Influence.bLake;
	Column.bOcean = Influence.bOcean;
	Column.bCoast = Influence.bCoast;
	Surface->ResolveColumn(Column);
	OutColumn = Column;
	return true;
}

TConstArrayView<FVoxelStructurePlanPtr> FVoxelGenerationQuery::GetPreparedStructurePlans() const
{
	return PreparedStructures;
}

bool FVoxelGenerationQuery::SampleColumn(int32 InX, int32 InY, FVoxelColumnSample& OutColumn, FString& OutError) const
{
	if (!bPrepared)
	{
		OutError = TEXT("Voxel generation query is not prepared");
		return false;
	}
	if (!SampleBaseColumn(InX, InY, OutColumn)) return false;
	const FVoxelAquiferSample Sample = Aquifer->Sample(FIntVector(InX, InY, OutColumn.SurfaceZ), OutColumn, 0);
	OutColumn.GroundWaterZ = Sample.SurfaceZ;
	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::SampleSymbol(const FIntVector& InPosition, uint32& OutValue, FString& OutError) const
{
	if (!bPrepared || !PreparedBounds.Contains(InPosition))
	{
		OutError = TEXT("Voxel generation position is outside prepared bounds");
		return false;
	}
	FVoxelColumnSample Column;
	if (!SampleBaseColumn(InPosition.X, InPosition.Y, Column)) return false;
	uint32 Value = Config->Recipe->Palette.Air;
	static constexpr EVoxelGenerationStage Stages[] =
	{
		EVoxelGenerationStage::TerrainDensity, EVoxelGenerationStage::Aquifer, EVoxelGenerationStage::Surface,
		EVoxelGenerationStage::Carving, EVoxelGenerationStage::HydrologyRaster,
		EVoxelGenerationStage::UndergroundStructures, EVoxelGenerationStage::SurfaceStructures,
		EVoxelGenerationStage::UndergroundOres, EVoxelGenerationStage::UndergroundDecoration,
		EVoxelGenerationStage::FluidFeatures, EVoxelGenerationStage::Vegetation,
		EVoxelGenerationStage::SurfaceDecoration, EVoxelGenerationStage::TopLayer
	};
	for (const EVoxelGenerationStage Stage : Stages)
	{
		if (!ApplyStage(Stage, InPosition, Column, Value, OutError)) return false;
	}
	OutValue = Value;
	OutError.Reset();
	return true;
}

bool FVoxelGenerationQuery::ApplyStage(EVoxelGenerationStage InStage, const FIntVector& Position,
	FVoxelColumnSample& Column, uint32& Value, FString& OutError) const
{
	const FVoxelGenerationPalette& Palette = Config->Recipe->Palette;
	switch (InStage)
	{
	case EVoxelGenerationStage::TerrainDensity:
	{
		const FVoxelMacroTerrainSample Macro{Column.SurfaceZ, Column.DensityHeight, Column.SlopePermille};
		Value = Terrain->SampleDensityQ16(Position, Macro) > 0 ? Palette.Stone : Palette.Air;
		break;
	}
	case EVoxelGenerationStage::Aquifer:
		if ((Value & 0xffffu) == Palette.Air)
		{
			const FVoxelAquiferSample Sample = Aquifer->Sample(Position, Column, -1);
			if (Sample.Fluid == EVoxelFluidKind::Water) Value = Palette.Water;
			else if (Sample.Fluid == EVoxelFluidKind::Lava) Value = Palette.Lava;
		}
		break;
	case EVoxelGenerationStage::Surface:
		if ((Value & 0xffffu) != Palette.Air && Position.Z <= Column.SurfaceZ)
		{
			Value = Surface->ResolveSymbol(Position, Column, Column.SurfaceZ - Position.Z);
		}
		break;
	case EVoxelGenerationStage::Carving:
		for (const FVoxelCavePlanPtr& Plan : PreparedCaves)
		{
			if (Plan && !Plan->ProtectsFloor(Position) && Plan->Carves(Position))
			{
				Value = Palette.Air;
				break;
			}
		}
		break;
	case EVoxelGenerationStage::HydrologyRaster:
		if (Column.SurfaceWaterZ != MIN_int32 && Position.Z > Column.SurfaceZ && Position.Z <= Column.SurfaceWaterZ)
		{
			Value = Palette.Water;
		}
		break;
	case EVoxelGenerationStage::UndergroundStructures:
	case EVoxelGenerationStage::SurfaceStructures:
		for (const FVoxelStructurePlanPtr& Plan : PreparedStructures)
		{
			if (!Plan) continue;
			if (Plan->IsCleared(Position, InStage)) Value = Palette.Air;
			uint32 PlannedValue = 0;
			if (Plan->Sample(Position, InStage, PlannedValue)) Value = PlannedValue;
		}
		break;
	default:
		for (const FVoxelFeaturePlanPtr& Plan : PreparedFeatures)
		{
			uint32 PlannedValue = 0;
			if (Plan && Plan->Sample(Position, InStage, PlannedValue)) Value = PlannedValue;
		}
		break;
	}
	OutError.Reset();
	return true;
}
