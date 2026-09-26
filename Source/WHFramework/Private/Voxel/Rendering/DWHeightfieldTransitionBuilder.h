#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/DWLodTransition.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"

struct FVoxelGenerationRuntimeConfig;
struct FVoxelMacroTileData;
struct FVoxelRegistrySnapshot;
struct FVoxelSurfaceTileData;

struct FVoxelHeightfieldTileView
{
	FVoxelHeightfieldNodeKey Key;
	FIntPoint Origin = FIntPoint::ZeroValue;
	int32 Side = 0;
	int32 Step = 1;
	uint64 Revision = 0;
	TConstArrayView<int32> Ground;
	TConstArrayView<int32> Water;
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
		const TAtomic<bool>* InCancel = nullptr);
};
