#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"

class WHFRAMEWORK_API FVoxelHeightfieldPageBuild
{
public:
	bool Initialize(const FVoxelTerrainGeometry& InGeometry, FString& OutError);
	bool Advance(const FVoxelSurfaceProxyBuilder& InBuilder, FVoxelSurfaceTileData& OutData,
		bool& bOutComplete, FString& OutError, const TAtomic<bool>* InCancel);
	bool Advance(const FVoxelMacroTerrainBuilder& InBuilder, FVoxelMacroTileData& OutData,
		bool& bOutComplete, FString& OutError, const TAtomic<bool>* InCancel);
	uint64 MaximumRetainedBytes() const;
	uint64 MaximumDistantBytes() const;

private:
	FVoxelTerrainGeometry Geometry;
	TArray<FVoxelTerrainBuildShard> Shards;
	FVoxelSurfaceTileData SurfacePage;
	FVoxelMacroTileData MacroPage;
	bool bAllocated = false;
	int32 NextShard = 0;
};
