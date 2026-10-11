#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/AssetManagerTypes.h"
#include "InventoryTypes.generated.h"

class FInventoryItemRegistry;

UENUM(BlueprintType)
enum class EInventoryPropertyType : uint8
{
    None,
    Integer,
    Boolean,
    Guid,
    Name
};

UENUM(BlueprintType)
enum class EInventoryStackPolicy : uint8
{
    None,
    Stackable,
    Unique
};

UENUM(BlueprintType)
enum class EInventoryUsePolicy : uint8
{
    None,
    Consumable,
    ReusableTool,
    Equip,
    SkillSocket,
    PlaceVoxel,
    Recipe,
    QuestKey
};

UENUM(BlueprintType)
enum class EInventoryHandPolicy : uint8
{
    None,
    OneHand,
    TwoHand
};

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
struct WHFRAMEWORK_API FInventoryItemProperty
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Key;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SchemaVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EInventoryPropertyType Type = EInventoryPropertyType::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 IntValue = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool BoolValue = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGuid GuidValue;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName NameValue;

    bool operator==(const FInventoryItemProperty& InOther) const;
    bool operator!=(const FInventoryItemProperty& InOther) const;
    int32 Compare(const FInventoryItemProperty& InOther) const;
    bool HasCanonicalFields() const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryStackSignature
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FPrimaryAssetId ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Level = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGuid InstanceId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FInventoryItemProperty> Properties;

    bool operator==(const FInventoryStackSignature& InOther) const;
    bool operator!=(const FInventoryStackSignature& InOther) const;
    int32 Compare(const FInventoryStackSignature& InOther) const;
    friend WHFRAMEWORK_API uint32 GetTypeHash(const FInventoryStackSignature& InSignature);
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAbilityItem
{
    GENERATED_BODY()

    FAbilityItem();
    FAbilityItem(const FPrimaryAssetId& InItemId, int32 InCount = 1, int32 InLevel = 0);

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FPrimaryAssetId ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Count = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Level = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGuid InstanceId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FInventoryItemProperty> Properties;

    bool IsEmpty() const;
    bool IsValid(const FInventoryItemRegistry& InRegistry) const;
    bool CanStackWith(const FAbilityItem& InOther) const;
    FInventoryStackSignature Signature() const;
    const FInventoryItemProperty* FindProperty(FName InKey) const;
    FGuid GetBoundOwner() const;
    bool operator==(const FAbilityItem& InOther) const;
    bool operator!=(const FAbilityItem& InOther) const;
};

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

using FInventoryState = FInventorySnapshot;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInventoryItemFilter
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FPrimaryAssetId ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bMatchLevel = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Level = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bMatchSignature = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FInventoryStackSignature Signature;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bIncludeUnique = true;

    bool Matches(const FAbilityItem& InItem) const;
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

struct WHFRAMEWORK_API FInventoryOperationResult
{
    EInventoryResultCode Code = EInventoryResultCode::None;
    int64 AcceptedCount = 0;
    int64 RemainingCount = 0;
};

struct WHFRAMEWORK_API FInventoryQuantityDelta
{
    FInventoryStackSignature Signature;
    int64 Minted = 0;
    int64 Consumed = 0;
};

struct WHFRAMEWORK_API FInventoryPlan
{
    EInventoryResultCode Code = EInventoryResultCode::None;
    int64 AcceptedCount = 0;
    int64 RemainingCount = 0;
    TArray<FInventorySnapshot> Before;
    TArray<FInventorySnapshot> After;
    FInventoryChangeSet Changes;
    TArray<FInventoryOperationResult> Operations;
    TArray<FInventoryQuantityDelta> QuantityDeltas;
};

struct WHFRAMEWORK_API FInventoryQueryRequest
{
    FInventorySlotKey SpecificSlot;
    TArray<FGameplayTag> AllowedGroups;
    FInventoryItemFilter Filter;
    FAbilityItem CapacityItem;
    bool bAutomatic = false;
};

struct WHFRAMEWORK_API FInventoryQueryResult
{
    int64 MatchingCount = 0;
    int64 Capacity = 0;
    uint64 ObservedRevision = 0;
    TArray<FInventorySlotState> Matches;
};

namespace InventoryOrder
{
    WHFRAMEWORK_API bool GuidLess(const FGuid& InLeft, const FGuid& InRight);
    WHFRAMEWORK_API bool SlotKeyLess(const FInventorySlotKey& InLeft, const FInventorySlotKey& InRight);
}
