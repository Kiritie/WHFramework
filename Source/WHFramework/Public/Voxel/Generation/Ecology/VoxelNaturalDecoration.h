#pragma once

#include "Voxel/Generation/VoxelGenerationRecipe.h"

/** Pure underground decoration stage. The cavity and protected-path predicates come from prepared cave plans. */
class WHFRAMEWORK_API FVoxelNaturalDecoration
{
public:
	static uint32 Resolve(const FVoxelGenerationRecipe& InRecipe, const FVoxelColumnSample& InColumn,
		const FIntVector& InPosition, uint32 InCurrentSymbol,
		TFunctionRef<bool(const FIntVector&)> InIsCavity, TFunctionRef<bool(const FIntVector&)> InIsProtected);
};
