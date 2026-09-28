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
		const FVoxelViewSettings& InViewSettings,
		const FVoxelInterestSet* InPrevious = nullptr) const;

	void BuildSourceExact(const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet& OutInterest, const FVoxelInterestSet* InPrevious = nullptr) const;
	void BuildSourceProxy(const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet& OutInterest) const;
	void BuildSourceSurface(const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet& OutInterest) const;
	void BuildSourceMacro(const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet& OutInterest) const;
	FVoxelInterestSet Finalize(TConstArrayView<FVoxelStreamingSource> InSources,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet InPrepared) const;

private:
	void AddExactSource(
		const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest,
		const FVoxelViewSettings& InViewSettings,
		FVoxelInterestSet& InOutInterest,
		const FVoxelInterestSet* InPrevious) const;
	void AddViewSource(
		const FVoxelStreamingSource& InSource,
		const FVoxelWorldManifest& InManifest,
		const FVoxelViewSettings& InViewSettings,
		FVoxelInterestSet& InOutInterest,
		bool bBuildProxy,
		bool bBuildSurface,
		bool bBuildMacro) const;
};
