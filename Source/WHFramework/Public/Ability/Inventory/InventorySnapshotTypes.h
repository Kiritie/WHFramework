#pragma once

#include "Ability/Inventory/InventoryItemTypes.h"
#include "InventorySnapshotTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventorySlotKey
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag Group;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Index = INDEX_NONE;

    bool IsValid() const;
    bool operator==(const FInventorySlotKey& InOther) const;
    bool operator!=(const FInventorySlotKey& InOther) const;
    friend WHFRAMEWORK_API uint32 GetTypeHash(const FInventorySlotKey& InKey);
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryInitialPlacement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FAbilityItem Item;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FInventorySlotKey TargetSlot;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventorySlotState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FInventorySlotKey Key;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FAbilityItem Item;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bFavorite = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bLocked = false;

    bool operator==(const FInventorySlotState& InOther) const;
    bool operator!=(const FInventorySlotState& InOther) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventorySnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    int32 SchemaVersion = 2;

    UPROPERTY(BlueprintReadOnly)
    FGuid InventoryId;

    UPROPERTY(BlueprintReadOnly)
    FGuid OwnerId;

    UPROPERTY(BlueprintReadOnly)
    FGuid WorldId;

    UPROPERTY(BlueprintReadOnly)
    FPrimaryAssetId ProfileId;

    UPROPERTY()
    uint32 Generation = 1;

    UPROPERTY()
    uint64 Revision = 0;

    UPROPERTY(BlueprintReadOnly)
    TArray<FInventorySlotState> Slots;

    const FInventorySlotState* FindSlot(const FInventorySlotKey& InKey) const;
    bool operator==(const FInventorySnapshot& InOther) const;
    bool operator!=(const FInventorySnapshot& InOther) const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryChangedSlot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid InventoryId;

    UPROPERTY(BlueprintReadOnly)
    FInventorySlotKey Key;

    UPROPERTY(BlueprintReadOnly)
    FInventorySlotState Before;

    UPROPERTY(BlueprintReadOnly)
    FInventorySlotState After;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryRevision
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid InventoryId;

    UPROPERTY()
    uint64 Before = 0;

    UPROPERTY()
    uint64 After = 0;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryChangeSet
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid TransactionId;

    UPROPERTY(BlueprintReadOnly)
    TArray<FInventoryChangedSlot> ChangedSlots;

    UPROPERTY(BlueprintReadOnly)
    TArray<FInventoryRevision> Revisions;
};

using FInventoryState = FInventorySnapshot;

namespace InventoryOrder
{
    WHFRAMEWORK_API bool SlotKeyLess(const FInventorySlotKey& InLeft, const FInventorySlotKey& InRight);
}
