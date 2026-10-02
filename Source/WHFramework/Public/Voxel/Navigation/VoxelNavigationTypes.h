#pragma once

#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionSnapshot.h"

struct FVoxelRegistrySnapshot;
class FVoxelShapeRegistry;

struct WHFRAMEWORK_API FVoxelNavigationSurface
{
	TArray<FVector> Vertices;
	TArray<int32> Indices;
	FBox Bounds = FBox(ForceInit);
};

class WHFRAMEWORK_API FVoxelNavigationSurfaceBuilder
{
public:
	static bool Build(const FVoxelSectionSnapshot& InSection, const FVoxelSectionSnapshot* InAbove,
		const FVoxelRegistrySnapshot& InRegistry, const FVoxelShapeRegistry& InShapes,
		double InCellSize, double InClearance, FVoxelNavigationSurface& OutSurface, const TAtomic<bool>* InCancel = nullptr);
};
