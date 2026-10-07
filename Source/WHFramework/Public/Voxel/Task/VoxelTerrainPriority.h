#pragma once

#include "CoreMinimal.h"
#include "Voxel/Task/VoxelTerrainWorkSelection.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

struct WHFRAMEWORK_API FVoxelTerrainTaskRole
{
	bool bInteractive = false;
	bool bCollisionGuard = false;
	bool bAheadPrefetch = false;
};

class WHFRAMEWORK_API FVoxelTerrainPriority
{
public:
	static bool Resolve(const FVoxelTerrainGeometry& InGeometry, const FVoxelTerrainTaskRole& InRole,
		TConstArrayView<FVoxelStreamingSource> InSources, int32 InP0RadiusCells,
		FVoxelTerrainServicePriority& OutPriority, FString& OutError);
};
