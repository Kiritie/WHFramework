#pragma once
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionStorage.h"
#include "Voxel/Save/VoxelDeltaTypes.h"
#include "Voxel/Tasks/VoxelTaskTypes.h"
enum class EVoxelSectionStatus : uint8
{
	Allocated,
	DataReady,
	Unloading,
	Failed
};
struct WHFRAMEWORK_API FVoxelSection
{
	FVoxelTaskStamp Stamp;
	EVoxelSectionStatus Status = EVoxelSectionStatus::Allocated;
	FVoxelSectionStorage Blocks;
	FVoxelSectionOverlay Overlay;
	uint64 CommittedRevision = 0;
	uint32 PinCount = 0;
	bool bMeshDirty = true;
	bool bCollisionDirty = true;
	bool bHasCollision = false;
	bool bWantsMesh = false;
	bool bWantsCollision = false;
	bool bWantsSimulation = false;
	double LastWanted = 0;
	bool IsSaveDirty() const
	{
		return Stamp.Revision != CommittedRevision;
	}
};
