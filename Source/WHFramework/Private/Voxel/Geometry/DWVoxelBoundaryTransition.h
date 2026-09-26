#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/DWVolumeTransitionPlanner.h"
#include "Voxel/Runtime/VoxelBlockState.h"

struct FVoxelSection;
struct FVoxelVoxelProxyData;

/** A frozen, single-face copy. No runtime section is read by a mesh worker. */
struct FVoxelBoundaryFaceSnapshot
{
	FVoxelViewKey Key;
	uint8 Face = 0;
	uint64 Revision = 0;
	TArray<uint32> States;

	bool Sample(const FIntVector& InWorldCell, FVoxelBlockState& OutState) const;

	static bool CaptureFine(const FVoxelViewKey& InKey, uint8 InFace,
		const FVoxelSection& InSection, FVoxelBoundaryFaceSnapshot& OutSnapshot);
	static bool CaptureProxy(const FVoxelVoxelProxyData& InData, uint8 InFace,
		FVoxelBoundaryFaceSnapshot& OutSnapshot);
};

struct FVoxelBoundaryTransitionPatch
{
	FVoxelVolumeTransitionFace Face;
	FVoxelBoundaryFaceSnapshot Neighbor;
};

struct FVoxelBoundaryTransitionContext
{
	FVoxelViewKey Owner;
	TArray<FVoxelBoundaryTransitionPatch> Patches;

	bool CoversCell(uint8 InFace, const FIntVector& InLocalCell) const;
	bool Validate() const;
	uint64 Signature(uint64 InOwnerRevision) const;
};
