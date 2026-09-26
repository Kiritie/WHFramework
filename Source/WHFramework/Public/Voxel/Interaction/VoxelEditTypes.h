#pragma once
#include "UObject/PrimaryAssetId.h"
#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Save/VoxelDeltaTypes.h"
enum class EVoxelEditAction:uint8{BreakBegin,BreakPulse,BreakCancel,Place,Use,ContainerTake,ContainerPut};
enum class EVoxelEditCode:uint8{Accepted,Pending,Rejected,NeedsData,Busy,Stale,InventoryFull};
struct WHFRAMEWORK_API FVoxelEditIntent
{
    uint64 RequestId=0;
    EVoxelEditAction Action=EVoxelEditAction::Use;
    FVector Origin=FVector::ZeroVector,Direction=FVector::ForwardVector;
    FIntVector ExpectedTarget=FIntVector::ZeroValue;
    FPrimaryAssetId ExpectedItemID;
    int32 InventorySlot=0,ContainerSlot=0,Count=1;
    uint64 ExpectedRevision=0;
};
struct WHFRAMEWORK_API FVoxelEditReply
{
    uint64 RequestId=0;EVoxelEditCode Code=EVoxelEditCode::Rejected;
    FString Reason;
};
struct WHFRAMEWORK_API FVoxelInteractionPlan
{
    FGuid ObjectId;
    FIntVector ObjectAnchor = FIntVector::ZeroValue;
    FName ObjectAction;
    TArray<FVoxelCellEdit> Cells;
    TArray<FVoxelEntityEdit> Entities;
    int32 Cost=0;
    FPrimaryAssetId DropID;
    int32 DropCount=0;
};
