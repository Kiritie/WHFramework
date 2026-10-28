#include "Voxel/Rendering/VoxelMacroTerrain.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"
#include "Voxel/Rendering/VoxelWaterView.h"
FVoxelMacroTerrainBuilder::FVoxelMacroTerrainBuilder(
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	const FVoxelGenerationSettings& InSettings, const IVoxelOverlaySource& InOverlays,
		TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
	const FVoxelLodTopologySettings& InLod)
	: Lod(InLod)
	, Config(InConfig)
	, SurfaceBuilder(InGenerator, InConfig, InSettings, InOverlays, InRegistry, InLod)
{
}

bool FVoxelMacroTerrainBuilder::Build(
	const FVoxelMacroTileKey& InKey,
	FVoxelMacroTileData& OutData,
	FString& OutError,
	const TAtomic<bool>* InCancel, const FVoxelTerrainGeometry& InGeometry, const FVoxelTerrainBuildShard* InShard) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_MacroBuild);

	FVoxelMacroTileData Data;

	Data.Key =
		InKey;

	Data.Geometry = InGeometry;
	if (!Data.Geometry.Validate(OutError)) return false;
	if ((Data.Geometry.Key.Representation != EVoxelTerrainRepresentation::Macro) ||
		Data.Geometry.Key.Coordinate != FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0) || Data.Geometry.Key.Level != InKey.Level)
	{ OutError = TEXT("Macro spatial key does not match its required geometry"); return false; }
	const int32 SampleSide = InShard ? InShard->CellCounts.X : int32(Data.Geometry.Key.SideCells()) / Data.Geometry.SampleStepCells;
	const FVoxelTerrainBuildShard Window = InShard ? *InShard :
		FVoxelTerrainBuildShard{FIntVector::ZeroValue, FIntVector(SampleSide, SampleSide, 0)};
	if (!Window.Validate(Data.Geometry, 256, OutError))
	{
		return false;
	}
	Data.Step = Data.Geometry.SampleStepCells;
	Data.LocalMinCells = InShard ? FIntPoint(InShard->LocalMinCells.X, InShard->LocalMinCells.Y) : FIntPoint::ZeroValue;
	Data.Side = SampleSide + 1;

	const int32 Count =
		Data.GetVertexCount();

	Data.Height.SetNumUninitialized(Count);
	Data.WaterHeight.SetNumUninitialized(Count);
	Data.SurfaceStates.SetNumUninitialized(Count);
	Data.ForestCoverage.SetNumUninitialized(Count);
	Data.SnowCoverage.SetNumUninitialized(Count);

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ColumnsLease;
	TArray<FVoxelColumnSample> Columns;
	FVoxelSurfaceTileData Surface;
	const FVoxelSurfaceTileKey SurfaceKey { InKey.Coordinate,
		InKey.Level };
	if (!SurfaceBuilder.Build(SurfaceKey, Surface, OutError, InCancel,
		nullptr, &Columns, Data.Geometry, InShard, &ColumnsLease))
	{
		return false;
	}

	if (Columns.Num() !=
		Count)
	{
		OutError =
			TEXT("Voxel macro column grid returned an invalid vertex count");

		return false;
	}
	Data.Revision = Surface.Revision;
	if (!FVoxelWaterViewBuilder::BuildKindGrid(
		Surface, Data.WaterKind, OutError))
	{
		return false;
	}

	const FVoxelEcologyGenerator Ecology(Config->Recipe.ToSharedRef());
	for (int32 Index = 0;
		Index < Count;
		++Index)
	{
		const FVoxelColumnSample& Column =
			Columns[Index];

		Data.Height[Index] = Surface.GroundZ[Index];

		Data.WaterHeight[Index] = Surface.WaterZ[Index];

		Data.SurfaceStates[Index] = Surface.SurfaceStates[Index];

		const FVoxelEcologySample EcologySample = Ecology.Sample(Column);

		const int32 Temperature =
			FMath::Clamp(
				Column.Climate.
					TemperatureQ15,
				-32768,
				32767);

		Data.ForestCoverage[Index] = static_cast<uint8>(
			EcologySample.bTreeAllowed ? EcologySample.TreeDensity * 255 / 1000 : 0);

		Data.SnowCoverage[Index] =
			static_cast<uint8>(
				FMath::Clamp(
					(-Temperature) *
						255 /
						32768,
					0,
					255));
	}
	Data.DistantCells = MoveTemp(Surface.DistantCells);

	OutData =
		MoveTemp(Data);

	OutError.Reset();
	return true;
}
