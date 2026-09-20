#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelWorldManifest.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

class WHFRAMEWORK_API FVoxelInterestManager
{
public:
	FVoxelInterestSet Compute(
		TConstArrayView<FVoxelStreamingSource> InSources,
		const FVoxelWorldManifest& InManifest,
		const FVoxelViewSettings& InViewSettings) const;

private:
	void AddExactSource(
		const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest,
		const FVoxelViewSettings& InViewSettings,
		FVoxelInterestSet& InOutInterest) const;
	void AddViewSource(
		const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest,
		const FVoxelViewSettings& InViewSettings,
		FVoxelInterestSet& InOutInterest) const;
};
