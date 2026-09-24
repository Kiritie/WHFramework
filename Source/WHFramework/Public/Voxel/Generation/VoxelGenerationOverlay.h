#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"

/** Immutable, worker-safe project composition applied to an exact generated section. */
class WHFRAMEWORK_API IVoxelGenerationOverlay
{
public:
	virtual ~IVoxelGenerationOverlay() = default;

	virtual bool ApplySection(
		const FIntVector& InSectionCoordinate,
		TArray<FVoxelBlockState>& InOutBlocks,
		FString& OutError,
		const TAtomic<bool>* InCancel) const = 0;
};
