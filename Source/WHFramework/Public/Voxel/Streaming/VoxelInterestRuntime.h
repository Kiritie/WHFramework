#pragma once

#include "CoreMinimal.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Generation/VoxelWorldManifest.h"

struct WHFRAMEWORK_API FVoxelInterestDelta
{
	TSet<FIntVector> AddedExact;
	TSet<FIntVector> RemovedExact;
	TSet<FIntVector> AddedFine;
	TSet<FIntVector> RemovedFine;
	TSet<FVoxelViewKey> AddedProxy;
	TSet<FVoxelViewKey> RemovedProxy;
	TSet<FVoxelSurfaceTileKey> AddedSurface;
	TSet<FVoxelSurfaceTileKey> RemovedSurface;
	TSet<FVoxelMacroTileKey> AddedMacro;
	TSet<FVoxelMacroTileKey> RemovedMacro;
	bool bVisualPartitionChanged = false;

	void Reset()
	{
		*this = {};
	}
};

class WHFRAMEWORK_API FVoxelInterestRuntime
{
public:
	static bool NeedsUpdate(const FVoxelStreamingSource& InPrevious,
		const FVoxelStreamingSource& InCurrent);

	bool Update(TConstArrayView<FVoxelStreamingSource> InSources,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet& OutInterest, FVoxelInterestDelta& OutDelta,
		bool& bOutSnapshot);

	void Reset();

private:
	struct FSourceState
	{
		FVoxelStreamingSource Source;
		FIntVector ExactAnchor = FIntVector(MAX_int32);
		FIntVector FineAnchor = FIntVector(MAX_int32);
		FIntVector ProxyAnchor = FIntVector(MAX_int32);
		FIntPoint SurfaceAnchor = FIntPoint(MAX_int32);
		FIntPoint MacroAnchor = FIntPoint(MAX_int32);
		FIntVector PredictionAnchor = FIntVector(MAX_int32);
		FVoxelInterestSet Exact;
		FVoxelInterestSet Fine;
		FVoxelInterestSet Proxy;
		FVoxelInterestSet Surface;
		FVoxelInterestSet Macro;
	};

	static FIntVector Snap3D(const FIntVector& InCell, int32 InStep);
	static FIntPoint Snap2D(const FIntVector& InCell, int32 InStep);
	static bool ConfigurationChanged(const FVoxelStreamingSource& InA,
		const FVoxelStreamingSource& InB);

	TMap<FGuid, FSourceState> States;
	FVoxelInterestSet LatestInterest;
};
