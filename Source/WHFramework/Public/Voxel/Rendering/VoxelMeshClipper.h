#pragma once

#include "CoreMinimal.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"

namespace VoxelMeshClipper
{
	WHFRAMEWORK_API void NormalizeBoxes(TArray<FBox>& InOutBoxes);
	WHFRAMEWORK_API void Subtract(
		const FVoxelSectionMeshResult& InMesh,
		TConstArrayView<FBox> InBoxes,
		FVoxelSectionMeshResult& OutMesh,
		const TAtomic<bool>* InCancel = nullptr);
}
