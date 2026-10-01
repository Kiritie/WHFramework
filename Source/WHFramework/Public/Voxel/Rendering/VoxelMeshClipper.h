#pragma once

#include "CoreMinimal.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"

namespace VoxelMeshClipper
{
	WHFRAMEWORK_API void NormalizeBoxes(TArray<FBox>& InOutBoxes);
	WHFRAMEWORK_API void FindChangedCoverageBounds(
		TConstArrayView<FBox> InPrevious,
		TConstArrayView<FBox> InCurrent,
		const FBox& InSourceBounds,
		TArray<FBox>& OutBounds,
		const TAtomic<bool>* InCancel = nullptr);
	WHFRAMEWORK_API void Subtract(
		const FVoxelSectionMeshResult& InMesh,
		TConstArrayView<FBox> InBoxes,
		FVoxelSectionMeshResult& OutMesh,
		const TAtomic<bool>* InCancel = nullptr);
}
