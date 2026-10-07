#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelGameThreadContinuation.h"
#include "Voxel/Rendering/DWLodTransition.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/VoxelHeightfieldMesher.h"

struct FVoxelGenerationRuntimeConfig;
struct FVoxelMacroTileData;
struct FVoxelRegistrySnapshot;
struct FVoxelSurfaceTileData;

struct FVoxelFrozenHeightfieldSource
{
	FVoxelHeightfieldNodeKey Key;
	TSharedPtr<const FVoxelSurfaceTileData, ESPMode::ThreadSafe> Surface;
	TSharedPtr<const FVoxelMacroTileData, ESPMode::ThreadSafe> Macro;
	bool operator==(const FVoxelFrozenHeightfieldSource& InOther) const = default;
};

struct FVoxelFrozenHeightfieldInputs
{
	FVoxelHeightfieldNodeKey Owner;
	uint64 WorldEpoch = 0;
	uint64 GeneratorIdentity = 0;
	uint64 RegistryRevision = 0;
	uint64 ShapeRevision = 0;
	uint64 InputSerial = 0;
	double FirstQueuedSeconds = 0.0;
	TArray<FVoxelFrozenHeightfieldSource> Sources;
	TArray<FVoxelHeightfieldTransitionEdge> Edges;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	bool SameInputs(const FVoxelFrozenHeightfieldInputs& InOther) const;
};

struct FVoxelHeightfieldTileView
{
	FVoxelHeightfieldNodeKey Key;
	FIntPoint Origin = FIntPoint::ZeroValue;
	int32 Side = 0;
	int32 Step = 1;
	uint64 Revision = 0;
	uint64 GeometryRevision = 0;
	TConstArrayView<int32> Ground;
	TConstArrayView<int32> Water;
	TConstArrayView<uint8> WaterKind;
	TConstArrayView<uint8> SurfaceFlags;
	uint8 GetWaterKind(int32 InIndex) const;
	int32 WaterKindCount() const { return SurfaceFlags.IsEmpty() ? WaterKind.Num() : SurfaceFlags.Num(); }
	TConstArrayView<uint16> Material;
	double ZBias = 0.0;

	int32 CellSide() const
	{
		return Side - 1;
	}

	int32 Index(const int32 InX, const int32 InY) const
	{
		return InX + InY * Side;
	}
};

struct FVoxelHeightfieldTileFootprint
{
	FVoxelHeightfieldNodeKey Key;
	FIntPoint Origin = FIntPoint::ZeroValue;
	int32 TileSide = 0;
	int32 Step = 1;
};

class FVoxelHeightfieldTransitionBuilder
{
public:
	static FVoxelHeightfieldTileView MakeView(const FVoxelSurfaceTileData& InData);
	static FVoxelHeightfieldTileView MakeView(const FVoxelMacroTileData& InData);

	static bool BuildEdges(
		TConstArrayView<FVoxelHeightfieldTileView> InTiles,
		TArray<FVoxelHeightfieldTransitionEdge>& OutEdges,
		FString& OutError);

	static void GatherOwnerDependencies(TConstArrayView<FVoxelHeightfieldTileView> InTiles,
		TConstArrayView<FVoxelHeightfieldTileFootprint> InPendingTiles,
		TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldNodeKey>>& OutDependencies);
	static FVoxelGameThreadContinuation GatherOwnerDependenciesSteps(TConstArrayView<FVoxelHeightfieldTileView> InTiles,
		TConstArrayView<FVoxelHeightfieldTileFootprint> InPendingTiles, TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldNodeKey>>& OutDependencies);

	static void ExcludeCoveredIntervals(
		TConstArrayView<FVoxelHeightfieldTileView> InTiles,
		TConstArrayView<FBox> InFineBoxes,
		TConstArrayView<FBox> InProxySurfaceBoxes,
		TArray<FVoxelHeightfieldTransitionEdge>& InOutEdges);

	static void AppendPendingEdges(
		TConstArrayView<FVoxelHeightfieldTileView> InReadyTiles,
		TConstArrayView<FVoxelHeightfieldTileFootprint> InPendingTiles,
		TArray<FVoxelHeightfieldTransitionEdge>& InOutEdges);

	static uint64 BuildSignature(
		const FVoxelHeightfieldNodeKey& InOwner,
		uint64 InOwnerRevision,
		TConstArrayView<FVoxelHeightfieldTransitionEdge> InEdges);

	static bool BuildMesh(
		const FVoxelHeightfieldTileView& InOwner,
		TConstArrayView<FVoxelHeightfieldTransitionEdge> InEdges,
		const TMap<FVoxelHeightfieldNodeKey, FVoxelHeightfieldTileView>& InViews,
		const FVoxelGenerationRuntimeConfig& InConfig,
		const FVoxelRegistrySnapshot& InRegistry,
		int32 InMinimumGroundZ,
		double InMaximumTextureStretchCells,
		FVoxelSectionMeshResult& OutMesh,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr,
		FVoxelHeightfieldMesher::FCapacityScope* InCapacity = nullptr);
};
