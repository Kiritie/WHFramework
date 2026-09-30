#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"

enum class EVoxelWorkClass : uint8;

struct WHFRAMEWORK_API FVoxelOverlayPreflightStatus
{
	bool bReady = true;
	bool bFailed = false;
	FString Error;
};

/** Immutable, worker-safe project composition applied to an exact generated section. */
class WHFRAMEWORK_API IVoxelGenerationOverlay
{
public:
	virtual ~IVoxelGenerationOverlay() = default;

	/** Called on the admission thread before a section occupies a generation worker. */
	virtual FVoxelOverlayPreflightStatus EnsureSectionReady(
		const FIntVector& InSectionCoordinate,
		EVoxelWorkClass InWorkClass,
		int32 InSourcePriority,
		double InDistanceScore,
		double InForwardScore) const
	{
		return {};
	}

	virtual bool ApplySection(
		const FIntVector& InSectionCoordinate,
		TArray<FVoxelBlockState>& InOutBlocks,
		FString& OutError,
		const TAtomic<bool>* InCancel) const = 0;

	virtual bool BuildCoarse(
		const FIntVector&,
		int32,
		int32,
		TMap<FIntVector, FVoxelBlockState>& OutCells,
		FString& OutError,
		const TAtomic<bool>*) const
	{
		OutCells.Reset();
		OutError.Reset();
		return true;
	}
};
