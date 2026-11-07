#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/AssetManagerTypes.h"
#include "InventoryItemTypes.generated.h"

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

namespace InventoryOrder
{
    WHFRAMEWORK_API bool GuidLess(const FGuid& InLeft, const FGuid& InRight);
}
