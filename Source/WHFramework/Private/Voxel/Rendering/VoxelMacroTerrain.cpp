#include "Voxel/Rendering/VoxelMacroTerrain.h"

namespace
{
	constexpr int32 MacroGridSide = 32;
	constexpr int32 MacroBaseStep = 64;
}

FVoxelMacroTerrainBuilder::FVoxelMacroTerrainBuilder(
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator)
	: Generator(InGenerator)
{
}

bool FVoxelMacroTerrainBuilder::Build(
	const FVoxelMacroTileKey& InKey,
	FVoxelMacroTileData& OutData,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	FVoxelMacroTileData Data;
	Data.Key = InKey;
	Data.Side = MacroGridSide;
	Data.Step = MacroBaseStep << InKey.Level;
	const int32 Count = Data.Side * Data.Side;
	Data.Height.SetNumUninitialized(Count);
	Data.WaterHeight.SetNumUninitialized(Count);
	Data.SurfaceClass.SetNumUninitialized(Count);
	Data.ForestCoverage.SetNumUninitialized(Count);
	Data.SnowCoverage.SetNumUninitialized(Count);
	const FIntPoint TileOrigin = InKey.Coordinate * (Data.Side * Data.Step);

	for (int32 Y = 0; Y < Data.Side; ++Y)
	{
		for (int32 X = 0; X < Data.Side; ++X)
		{
			if (InCancel && InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}
			const FIntPoint World = TileOrigin + FIntPoint(
				X * Data.Step + Data.Step / 2,
				Y * Data.Step + Data.Step / 2);
			FVoxelColumnSample Column;
			if (!Generator->SampleColumn(World.X, World.Y, Column, OutError, InCancel))
			{
				return false;
			}

			const int32 Index = X + Y * Data.Side;
			Data.Height[Index] = Column.SurfaceZ;
			Data.WaterHeight[Index] = Column.SurfaceWaterZ;
			Data.SurfaceClass[Index] = Column.SurfaceMaterial;
			const int32 Moisture = FMath::Clamp(Column.Climate.MoistureQ15, 0, 32767);
			const int32 Temperature = FMath::Clamp(Column.Climate.TemperatureQ15, -32768, 32767);
			Data.ForestCoverage[Index] = static_cast<uint8>(FMath::Clamp(Moisture * 255 / 32767, 0, 255));
			Data.SnowCoverage[Index] = static_cast<uint8>(FMath::Clamp((-Temperature) * 255 / 32768, 0, 255));
		}
	}

	OutData = MoveTemp(Data);
	OutError.Reset();
	return true;
}
