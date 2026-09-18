#pragma once
#include "CoreMinimal.h"
#include "Voxel/Tasks/VoxelTaskTypes.h"
#include "Voxel/Chunks/VoxelSectionStorage.h"
#include "Voxel/Save/VoxelDeltaTypes.h"
enum class EVoxelSectionStatus:uint8{Allocated,DataReady,Unloading,Failed};
struct WHFRAMEWORK_API FVoxelSection
{
    FVoxelTaskStamp Stamp;
    EVoxelSectionStatus Status=EVoxelSectionStatus::Allocated;
    FVoxelSectionStorage Blocks;
    TSharedPtr<const FVoxelSectionStorage,ESPMode::ThreadSafe> BaseBlocks;
    FVoxelSectionOverlay Overlay;
    uint64 CommittedRevision=0;
    uint32 PinCount=0;
    bool bMeshDirty=true,bCollisionDirty=true,bHasCollision=false;
    bool bWantsMesh=false,bWantsCollision=false,bWantsSimulation=false;
    double LastWanted=0;
    bool IsSaveDirty()const{return Stamp.Revision!=CommittedRevision;}
};
