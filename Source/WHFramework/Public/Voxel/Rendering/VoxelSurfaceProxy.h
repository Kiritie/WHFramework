#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Authoring/VoxelViewProfile.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"
#include "Voxel/Rendering/VoxelViewTypes.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Runtime/VoxelChangeIndex.h"
#include "Voxel/Runtime/VoxelOverlaySnapshot.h"

enum EVoxelSurfaceFlags : uint8
{
	VoxelSurface_River = 1 << 0,
	VoxelSurface_Lake = 1 << 1,
	VoxelSurface_Ocean = 1 << 2,
	VoxelSurface_Coast = 1 << 3
};

struct WHFRAMEWORK_API FVoxelDistantCell
{
	FIntVector Min = FIntVector::ZeroValue;
	FIntVector Max = FIntVector::ZeroValue;
	FVoxelBlockState State;
	FVoxelStableId SourceTreeId;
};

struct WHFRAMEWORK_API FVoxelSurfaceTileData
{
	static constexpr int32 CellSide = 32;
	static constexpr int32 VertexSide = CellSide + 1;

	FVoxelSurfaceTileKey Key;
	FVoxelTerrainGeometry Geometry;
	uint64 RequestId = 1;
	// Derived locally using the current material and emission rules; never part of wire identity.
	uint64 EstimatedMeshBytes = 0;
	FIntPoint LocalMinCells = FIntPoint::ZeroValue;
	FIntPoint GetBuildOrigin() const
	{
		return Key.Coordinate * int32(Geometry.Key.SideCells()) + LocalMinCells;
	}
	int32 Side = VertexSide;
	int32 Step = 0;
	uint64 Revision = 0;
	TArray<int32> GroundZ;
	TArray<int32> WaterZ;
	TArray<uint16> SurfaceMaterial;
	TArray<uint16> Biome;
	TArray<uint8> Flags;
	TArray<FVoxelDistantCell> DistantCells;

	int32 GetTileSide() const
	{
		return (Side - 1) * Step;
	}

	int32 GetVertexCount() const
	{
		return Side *
			Side;
	}
};

struct WHFRAMEWORK_API FVoxelSurfaceBuildTiming
{
	double ColumnsMilliseconds = 0.0;
	double OverlayMilliseconds = 0.0;
};

class WHFRAMEWORK_API FVoxelSurfaceProxyBuilder
{
public:
	FVoxelSurfaceProxyBuilder(
		TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> InGenerator,
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		const FVoxelGenerationSettings& InSettings,
		const IVoxelOverlaySource& InOverlaySource,
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> InRegistry,
		const FVoxelLodTopologySettings& InLod = FVoxelLodTopologySettings());

	bool Build(
		const FVoxelSurfaceTileKey& InKey,
		FVoxelSurfaceTileData& OutData,
		FString& OutError,
		const TAtomic<bool>* InCancel,
		FVoxelSurfaceBuildTiming* OutTiming, TArray<FVoxelColumnSample>* OutColumns,
		const FVoxelTerrainGeometry& InGeometry,
		const FVoxelTerrainBuildShard* InShard = nullptr) const;

private:
	bool CaptureModifiedBlocks(
		const FVoxelSurfaceTileData& InData,
		TMap<FIntVector, FVoxelBlockState>& OutBlocks,
		uint64& OutRevision,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;

	bool ApplyModifiedSurface(
		const FVoxelSurfaceTileKey& InKey,
		FVoxelSurfaceTileData& InOutData,
		const TMap<FIntVector, FVoxelBlockState>& InModifiedBlocks,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;

	bool BuildDistantCells(
		FVoxelSurfaceTileData& InOutData,
		const TMap<FIntVector, FVoxelBlockState>& InModifiedBlocks,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;

private:
	FVoxelLodTopologySettings Lod;
	TSharedRef<const FVoxelGenerationPipeline, ESPMode::ThreadSafe> Generator;
	TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	FVoxelGenerationSettings Settings;
	const IVoxelOverlaySource& OverlaySource;
	TSharedRef<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe> Registry;
};
