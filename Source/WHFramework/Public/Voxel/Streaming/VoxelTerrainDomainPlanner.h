#pragma once

#include "CoreMinimal.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Generation/VoxelWorldManifest.h"

class WHFRAMEWORK_API FVoxelTerrainDomainPlanner
{
public:
	static bool Build(TConstArrayView<FVoxelStreamingSource> InSources,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet& InOutInterest, FString& OutError);
};
