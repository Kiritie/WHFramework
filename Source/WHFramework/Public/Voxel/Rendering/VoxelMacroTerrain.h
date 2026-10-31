#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Authoring/VoxelViewProfile.h"
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
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	static constexpr int32 CellSide = 32;
	static constexpr int32 VertexSide = CellSide + 1;

	FVoxelMacroTileKey Key;
	FVoxelTerrainGeometry Geometry;
	uint64 RequestId = 1;
	// Derived locally using the current material and emission rules; never part of wire identity.
	uint64 EstimatedMeshBytes = 0;
	FIntPoint LocalMinCells = FIntPoint::ZeroValue;
	FIntPoint GetBuildOrigin() const
	{
		return Key.Coordinate * int32(Geometry.Key.SideCells()) + LocalMinCells;
	}
	uint64 Revision = 0;
	int32 Side = VertexSide;
	int32 Step = 0;
	TArray<int32> Height;
	TArray<int32> WaterHeight;
	TArray<uint8> WaterKind;
	TArray<FVoxelBlockState> SurfaceStates;
	TArray<uint8> ForestCoverage;
	TArray<FVoxelMacroStructureProxy> LargeStructures;
	TArray<FVoxelDistantCell> DistantCells;

	int32 GetTileSide() const
	{
		return (Side - 1) * Step;
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
		TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
		const FVoxelLodTopologySettings& InLod = FVoxelLodTopologySettings());

	bool Build(
		const FVoxelMacroTileKey& InKey,
		FVoxelMacroTileData& OutData,
		FString& OutError,
		const TAtomic<bool>* InCancel,
		const FVoxelTerrainGeometry& InGeometry,
		const FVoxelTerrainBuildShard* InShard = nullptr) const;

private:
	FVoxelLodTopologySettings Lod;
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	FVoxelSurfaceProxyBuilder SurfaceBuilder;
};
