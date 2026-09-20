#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelViewTypes.h"

struct WHFRAMEWORK_API FVoxelExactDemand
{
	bool bData = false;
	bool bExact = false;
	bool bCollision = false;
	bool bSimulation = false;
	bool bFineRender = false;
	bool bWarmupData = false;
	bool bWarmupCollision = false;
	double Priority = 0.0;
};

struct WHFRAMEWORK_API FVoxelInterestSet
{
	TMap<FIntVector, FVoxelExactDemand> Exact;
	TSet<FVoxelViewKey> VoxelProxy;
	TSet<FVoxelSurfaceTileKey> Surface;
	TSet<FVoxelMacroTileKey> Macro;
};

struct WHFRAMEWORK_API FVoxelViewSettings
{
	int32 WarmupDataRadius = 24;
	int32 WarmupCollisionRadius = 24;
	int32 FineRadius = 320;
	int32 VoxelProxyRadius = 3200;
	int32 SurfaceRadius = 32000;
	int32 MacroRadius = 120000;
	int32 VoxelProxyTileSide = 16;
	int32 SurfaceTileSide = 32;
	int32 MacroTileSide = 2048;
	float TargetScreenErrorPixels = 2.0f;
	uint8 MaximumVoxelProxyLevel = 4;
	uint8 MaximumSurfaceLevel = 4;
	uint8 MaximumMacroLevel = 4;
};
