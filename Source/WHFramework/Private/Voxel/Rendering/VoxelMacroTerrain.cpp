#include "Voxel/Rendering/VoxelMacroTerrain.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
FVoxelMacroTerrainBuilder::FVoxelMacroTerrainBuilder(
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
	const FVoxelGenerationSettings& InSettings, const IVoxelOverlaySource& InOverlays,
		TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry)
	: SurfaceBuilder(InGenerator, InConfig, InSettings, InOverlays, InRegistry)
{
}

bool FVoxelMacroTerrainBuilder::Build(
	const FVoxelMacroTileKey& InKey,
	FVoxelMacroTileData& OutData,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_MacroBuild);

	FVoxelMacroTileData Data;

	Data.Key =
		InKey;

	Data.Side =
		FVoxelMacroTileData::VertexSide;

	Data.Step =
		FVoxelMacroTileData::BaseStep <<
		InKey.Level;

	const int32 Count =
		Data.GetVertexCount();

	Data.Height.SetNumUninitialized(Count);
	Data.WaterHeight.SetNumUninitialized(Count);
	Data.SurfaceClass.SetNumUninitialized(Count);
	Data.ForestCoverage.SetNumUninitialized(Count);
	Data.SnowCoverage.SetNumUninitialized(Count);

	TArray<FVoxelColumnSample> Columns;
	FVoxelSurfaceTileData Surface;
	const FVoxelSurfaceTileKey SurfaceKey { InKey.Coordinate,
		static_cast<uint8>(InKey.Level + FMath::FloorLog2(FVoxelMacroTileData::BaseStep)) };
	if (!SurfaceBuilder.Build(SurfaceKey, Surface, OutError, InCancel, nullptr, &Columns))
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

	for (int32 Index = 0;
		Index < Count;
		++Index)
	{
		const FVoxelColumnSample& Column =
			Columns[Index];

		Data.Height[Index] = Surface.GroundZ[Index];

		Data.WaterHeight[Index] = Surface.WaterZ[Index];

		Data.SurfaceClass[Index] = Surface.SurfaceMaterial[Index];

		const int32 Moisture =
			FMath::Clamp(
				Column.Climate.
					MoistureQ15,
				0,
				32767);

		const int32 Temperature =
			FMath::Clamp(
				Column.Climate.
					TemperatureQ15,
				-32768,
				32767);

		Data.ForestCoverage[Index] =
			static_cast<uint8>(
				FMath::Clamp(
					Moisture *
						255 /
						32767,
					0,
					255));

		Data.SnowCoverage[Index] =
			static_cast<uint8>(
				FMath::Clamp(
					(-Temperature) *
						255 /
						32768,
					0,
					255));
	}

	OutData =
		MoveTemp(Data);

	OutError.Reset();
	return true;
}
