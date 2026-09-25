#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"

struct WHFRAMEWORK_API FVoxelMacroStructureProxy
{
	FIntPoint Coordinate = FIntPoint::ZeroValue;
	int32 GroundZ = 0;
	uint16 StructureIndex = 0;
	uint16 Radius = 0;
};

struct WHFRAMEWORK_API FVoxelMacroTileData
{
	static constexpr int32 CellSide = 32;
	static constexpr int32 VertexSide = CellSide + 1;
	static constexpr int32 BaseStep = 64;

	FVoxelMacroTileKey Key;
	uint64 Revision = 0;
	int32 Side = VertexSide;
	int32 Step = 0;
	TArray<int32> Height;
	TArray<int32> WaterHeight;
	TArray<uint16> SurfaceClass;
	TArray<uint8> ForestCoverage;
	TArray<uint8> SnowCoverage;
	TArray<FVoxelMacroStructureProxy> LargeStructures;
	TArray<FVoxelDistantCell> DistantCells;

	int32 GetTileSide() const
	{
		return CellSide * Step;
	}

	int32 GetVertexCount() const
	{
		return Side * Side;
	}
};

class WHFRAMEWORK_API FVoxelMacroTerrainBuilder
{
public:
	FVoxelMacroTerrainBuilder(TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		const FVoxelGenerationSettings& InSettings, const IVoxelOverlaySource& InOverlays,
		TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry);

	bool Build(
		const FVoxelMacroTileKey& InKey,
		FVoxelMacroTileData& OutData,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

private:
	FVoxelSurfaceProxyBuilder SurfaceBuilder;
};
