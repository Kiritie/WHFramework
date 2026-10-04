#pragma once

#include "CoreMinimal.h"

struct FVoxelStreamingSource;
struct FVoxelViewSettings;
struct FVoxelWorldManifest;
struct FVoxelInterestSet;

namespace VoxelFineDomain
{
	WHFRAMEWORK_API bool CompleteInitialFamilies(TConstArrayView<FVoxelStreamingSource> InSources,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet& InOutInterest, FString& OutError);
}
