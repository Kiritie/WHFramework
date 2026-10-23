#pragma once

#include "CoreMinimal.h"
#include "Ability/Inventory/InventoryTypes.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Save/VoxelDeltaTypes.h"

enum class EVoxelEditAction : uint8
{
    None,
    BreakBegin,
    BreakPulse,
    BreakCancel,
    Place,
    Use
};

enum class EVoxelEditCode : uint8
{
    None,
    Accepted,
    Pending,
    Rejected,
    NeedsData,
    Busy,
    Stale,
    InventoryFull,
    ConfirmationPending
};

struct WHFRAMEWORK_API FVoxelEditIntent
{
    uint64 RequestId = 0;
    FGuid InventoryStoreId;
    EVoxelEditAction Action = EVoxelEditAction::Use;
    FVector Origin = FVector::ZeroVector;
    FVector Direction = FVector::ForwardVector;
    FIntVector ExpectedTarget = FIntVector::ZeroValue;
    FPrimaryAssetId ExpectedItemID;
    FGuid ExpectedInventoryId;
    FInventorySlotKey InventorySlot;
    uint64 ExpectedInventoryRevision = 0;
    uint32 ExpectedInventoryGeneration = 1;
    uint64 ExpectedRevision = 0;
};

struct WHFRAMEWORK_API FVoxelEditReply
{
    uint64 RequestId = 0;
    EVoxelEditCode Code = EVoxelEditCode::Rejected;
    FString Reason;
};

struct WHFRAMEWORK_API FVoxelInteractionPlan
{
    EVoxelEditAction Action = EVoxelEditAction::None;
    uint64 RequestId = 0;
    FGuid EventId;
    FGuid ObjectId;
    FIntVector ObjectAnchor = FIntVector::ZeroValue;
    FName ObjectAction;
    FGuid InteractionInventoryId;
    TArray<FVoxelCellEdit> Cells;
    TArray<FVoxelEntityEdit> Entities;
    int32 Cost = 0;
    FPrimaryAssetId DropID;
    int32 DropCount = 0;
};
