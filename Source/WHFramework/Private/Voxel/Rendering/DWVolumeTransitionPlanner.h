#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelGameThreadContinuation.h"
#include "Voxel/Rendering/DWLodTransition.h"
#include "Voxel/Rendering/VoxelViewTypes.h"

struct FVoxelVolumeTransitionFace
{
	FVoxelViewKey Owner;
	FVoxelViewKey Neighbor;
	EVoxelVolumeFaceDirection Direction = EVoxelVolumeFaceDirection::PositiveX;
	FIntVector Min = FIntVector::ZeroValue;
	FIntVector Max = FIntVector::ZeroValue;
	int32 Ratio = 2;
};

class FVoxelVolumeTransitionPlanner
{
public:
	static bool SameInputs(const TSet<FVoxelViewKey>& InNodes, const TMap<FVoxelViewKey, int32>& InSampleSteps,
		const TSet<FVoxelViewKey>& InOtherNodes, const TMap<FVoxelViewKey, int32>& InOtherSampleSteps);
	static void GatherCoverageRegions(const TSet<FVoxelViewKey>& InVisible, uint8 InRegionLevel,
		TMap<FVoxelViewKey, TSet<FVoxelViewKey>>& OutRegions);
	static FVoxelGameThreadContinuation GatherCoverageRegionsSteps(const TSet<FVoxelViewKey>& InVisible, uint8 InRegionLevel,
		TMap<FVoxelViewKey, TSet<FVoxelViewKey>>& OutRegions);

	static void Build(
		const TSet<FVoxelViewKey>& InVisible,
		uint8 InMaximumLevel,
		TArray<FVoxelVolumeTransitionFace>& OutFaces,
		const TMap<FVoxelViewKey, int32>* InSampleSteps = nullptr,
		const FVoxelViewKey* InOwnerRegion = nullptr);
};
