#pragma once

#include "CoreMinimal.h"
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
	static void Build(
		const TSet<FVoxelViewKey>& InVisible,
		uint8 InMaximumLevel,
		TArray<FVoxelVolumeTransitionFace>& OutFaces);
};
