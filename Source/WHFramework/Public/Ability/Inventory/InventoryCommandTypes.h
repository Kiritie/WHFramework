#pragma once

#include "Ability/Inventory/InventorySnapshotTypes.h"
#include "InventoryCommandTypes.generated.h"

UENUM(BlueprintType)
enum class EInventoryResultCode : uint8
{
    None,
    Success,
    Partial,
    NoChange,
    InvalidRequest,
    InvalidItem,
    InvalidSlot,
    InvalidCount,
    InsufficientItems,
    NoCapacity,
    StackMismatch,
    SlotRuleRejected,
    ItemLocked,
    ItemBound,
    Unauthorized,
    OutOfRange,
    NotInteractable,
    InvalidState,
    InventoryGone,
    GenerationChanged,
    RevisionStale,
    AlreadyCommitted,
    SourcePreparationFailed,
    CommitFailed,
    WorldNotReady,
    SaveUnavailable,
    DuplicateInstance,
    RateLimited,
    ConfirmationPending
};

UENUM(BlueprintType)
enum class EInventoryOperationType : uint8
{
    None,
    Add,
    Remove,
    Move,
    Split,
    Merge,
    Swap,
    Transfer,
    Exchange,
    Sort,
    QuickStack,
    SetFavorite,
    Transform
};

UENUM(BlueprintType)
enum class EInventoryAmountMode : uint8
{
    None,
    Exact,
    UpTo
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryExchangeLine
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGuid SourceInventoryId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGuid TargetInventoryId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FInventorySlotKey SourceSlot;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FInventorySlotKey TargetSlot;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FGameplayTag> SourceGroups;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FGameplayTag> TargetGroups;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FInventoryItemFilter Filter;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Requested = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bAutomatic = true;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryOperation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EInventoryOperationType Type = EInventoryOperationType::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EInventoryAmountMode AmountMode = EInventoryAmountMode::Exact;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGuid SourceInventoryId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGuid TargetInventoryId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FInventorySlotKey SourceSlot;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FInventorySlotKey TargetSlot;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FGameplayTag> SourceGroups;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FGameplayTag> TargetGroups;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FGuid> TargetInventories;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FInventoryExchangeLine> ExchangeLines;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FInventoryItemFilter Filter;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FAbilityItem Item;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Requested = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bAutomatic = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bFavorite = false;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryCommandResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid TransactionId;

    UPROPERTY()
    uint64 RequestId = 0;

    UPROPERTY(BlueprintReadOnly)
    EInventoryResultCode Code = EInventoryResultCode::None;

    UPROPERTY(BlueprintReadOnly)
    int64 AcceptedCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int64 RemainingCount = 0;

    UPROPERTY(BlueprintReadOnly)
    TArray<FInventoryChangedSlot> ChangedSlots;

    UPROPERTY(BlueprintReadOnly)
    TArray<FInventoryRevision> FinalRevisions;

    bool IsSuccess() const;
};
