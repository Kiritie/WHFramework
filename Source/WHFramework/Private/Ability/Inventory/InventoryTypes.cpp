#include "Ability/Inventory/InventoryTypes.h"

#include "Ability/Inventory/InventoryItemRegistry.h"

namespace InventoryValueOrder
{
    int32 CompareNames(const FName InLeft, const FName InRight)
    {
        if (InLeft == InRight)
        {
            return 0;
        }
        return InLeft.ToString().Compare(InRight.ToString(), ESearchCase::IgnoreCase);
    }

    int32 CompareGuids(const FGuid& InLeft, const FGuid& InRight)
    {
        if (InLeft == InRight)
        {
            return 0;
        }
        return InventoryOrder::GuidLess(InLeft, InRight) ? -1 : 1;
    }
}

bool FInventoryItemProperty::operator==(const FInventoryItemProperty& InOther) const
{
    return Key == InOther.Key && SchemaVersion == InOther.SchemaVersion && Type == InOther.Type
        && IntValue == InOther.IntValue && BoolValue == InOther.BoolValue
        && GuidValue == InOther.GuidValue && NameValue == InOther.NameValue;
}

bool FInventoryItemProperty::operator!=(const FInventoryItemProperty& InOther) const
{
    return !(*this == InOther);
}

int32 FInventoryItemProperty::Compare(const FInventoryItemProperty& InOther) const
{
    const int32 KeyOrder = InventoryValueOrder::CompareNames(Key, InOther.Key);
    if (KeyOrder != 0)
    {
        return KeyOrder;
    }
    if (SchemaVersion != InOther.SchemaVersion)
    {
        return SchemaVersion < InOther.SchemaVersion ? -1 : 1;
    }
    if (Type != InOther.Type)
    {
        return static_cast<uint8>(Type) < static_cast<uint8>(InOther.Type) ? -1 : 1;
    }
    if (IntValue != InOther.IntValue)
    {
        return IntValue < InOther.IntValue ? -1 : 1;
    }
    if (BoolValue != InOther.BoolValue)
    {
        return BoolValue ? 1 : -1;
    }
    const int32 GuidOrder = InventoryValueOrder::CompareGuids(GuidValue, InOther.GuidValue);
    if (GuidOrder != 0)
    {
        return GuidOrder;
    }
    return InventoryValueOrder::CompareNames(NameValue, InOther.NameValue);
}

bool FInventoryItemProperty::HasCanonicalFields() const
{
    switch (Type)
    {
        case EInventoryPropertyType::Integer:
            return !BoolValue && !GuidValue.IsValid() && NameValue.IsNone();
        case EInventoryPropertyType::Boolean:
            return IntValue == 0 && !GuidValue.IsValid() && NameValue.IsNone();
        case EInventoryPropertyType::Guid:
            return IntValue == 0 && !BoolValue && NameValue.IsNone();
        case EInventoryPropertyType::Name:
            return IntValue == 0 && !BoolValue && !GuidValue.IsValid();
        default:
            return false;
    }
}

bool FInventoryStackSignature::operator==(const FInventoryStackSignature& InOther) const
{
    return ItemId == InOther.ItemId && Level == InOther.Level
        && InstanceId == InOther.InstanceId && Properties == InOther.Properties;
}

bool FInventoryStackSignature::operator!=(const FInventoryStackSignature& InOther) const
{
    return !(*this == InOther);
}

int32 FInventoryStackSignature::Compare(const FInventoryStackSignature& InOther) const
{
    if (ItemId != InOther.ItemId)
    {
        return ItemId.ToString().Compare(InOther.ItemId.ToString(), ESearchCase::IgnoreCase);
    }
    if (Level != InOther.Level)
    {
        return Level < InOther.Level ? -1 : 1;
    }
    const int32 GuidOrder = InventoryValueOrder::CompareGuids(InstanceId, InOther.InstanceId);
    if (GuidOrder != 0)
    {
        return GuidOrder;
    }
    const int32 SharedCount = FMath::Min(Properties.Num(), InOther.Properties.Num());
    for (int32 PropertyIndex = 0; PropertyIndex < SharedCount; ++PropertyIndex)
    {
        const int32 PropertyOrder = Properties[PropertyIndex].Compare(InOther.Properties[PropertyIndex]);
        if (PropertyOrder != 0)
        {
            return PropertyOrder;
        }
    }
    if (Properties.Num() == InOther.Properties.Num())
    {
        return 0;
    }
    return Properties.Num() < InOther.Properties.Num() ? -1 : 1;
}

uint32 GetTypeHash(const FInventoryStackSignature& InSignature)
{
    uint32 SignatureHash = HashCombine(GetTypeHash(InSignature.ItemId), GetTypeHash(InSignature.Level));
    SignatureHash = HashCombine(SignatureHash, GetTypeHash(InSignature.InstanceId));
    for (const FInventoryItemProperty& Property : InSignature.Properties)
    {
        SignatureHash = HashCombine(SignatureHash, GetTypeHash(Property.Key));
        SignatureHash = HashCombine(SignatureHash, GetTypeHash(Property.SchemaVersion));
        SignatureHash = HashCombine(SignatureHash, GetTypeHash(static_cast<uint8>(Property.Type)));
        SignatureHash = HashCombine(SignatureHash, GetTypeHash(Property.IntValue));
        SignatureHash = HashCombine(SignatureHash, GetTypeHash(Property.BoolValue));
        SignatureHash = HashCombine(SignatureHash, GetTypeHash(Property.GuidValue));
        SignatureHash = HashCombine(SignatureHash, GetTypeHash(Property.NameValue));
    }
    return SignatureHash;
}

FAbilityItem::FAbilityItem()
{
}

FAbilityItem::FAbilityItem(const FPrimaryAssetId& InItemId, const int32 InCount, const int32 InLevel)
    : ItemId(InItemId)
    , Count(InCount)
    , Level(InLevel)
{
}

bool FAbilityItem::IsEmpty() const
{
    return !ItemId.IsValid() && Count == 0 && Level == 0
        && !InstanceId.IsValid() && Properties.IsEmpty();
}

bool FAbilityItem::IsValid(const FInventoryItemRegistry& InRegistry) const
{
    FString Error;
    return !IsEmpty() && InRegistry.ValidateItem(*this, Error);
}

bool FAbilityItem::CanStackWith(const FAbilityItem& InOther) const
{
    return ItemId.IsValid() && InOther.ItemId.IsValid() && Count > 0 && InOther.Count > 0
        && !InstanceId.IsValid() && !InOther.InstanceId.IsValid() && Signature() == InOther.Signature();
}

FInventoryStackSignature FAbilityItem::Signature() const
{
    FInventoryStackSignature Result;
    Result.ItemId = ItemId;
    Result.Level = Level;
    Result.InstanceId = InstanceId;
    Result.Properties = Properties;
    return Result;
}

const FInventoryItemProperty* FAbilityItem::FindProperty(const FName InKey) const
{
    return Properties.FindByPredicate([InKey](const FInventoryItemProperty& InProperty)
    {
        return InProperty.Key == InKey;
    });
}

FGuid FAbilityItem::GetBoundOwner() const
{
    const FInventoryItemProperty* Property = FindProperty(FName(TEXT("BoundOwner")));
    if (Property && Property->Type == EInventoryPropertyType::Guid)
    {
        return Property->GuidValue;
    }
    return FGuid();
}

bool FAbilityItem::operator==(const FAbilityItem& InOther) const
{
    return Count == InOther.Count && Signature() == InOther.Signature();
}

bool FAbilityItem::operator!=(const FAbilityItem& InOther) const
{
    return !(*this == InOther);
}

bool FInventorySlotKey::IsValid() const
{
    return Group.IsValid() && Index >= 0;
}

bool FInventorySlotKey::operator==(const FInventorySlotKey& InOther) const
{
    return Group == InOther.Group && Index == InOther.Index;
}

bool FInventorySlotKey::operator!=(const FInventorySlotKey& InOther) const
{
    return !(*this == InOther);
}

uint32 GetTypeHash(const FInventorySlotKey& InKey)
{
    return HashCombine(GetTypeHash(InKey.Group), GetTypeHash(InKey.Index));
}

bool FInventorySlotState::operator==(const FInventorySlotState& InOther) const
{
    return Key == InOther.Key && Item == InOther.Item
        && bFavorite == InOther.bFavorite && bLocked == InOther.bLocked;
}

bool FInventorySlotState::operator!=(const FInventorySlotState& InOther) const
{
    return !(*this == InOther);
}

const FInventorySlotState* FInventorySnapshot::FindSlot(const FInventorySlotKey& InKey) const
{
    return Slots.FindByPredicate([&InKey](const FInventorySlotState& InSlot)
    {
        return InSlot.Key == InKey;
    });
}

bool FInventorySnapshot::operator==(const FInventorySnapshot& InOther) const
{
    return SchemaVersion == InOther.SchemaVersion && InventoryId == InOther.InventoryId
        && OwnerId == InOther.OwnerId && WorldId == InOther.WorldId
        && ProfileId == InOther.ProfileId && Generation == InOther.Generation
        && Revision == InOther.Revision && Slots == InOther.Slots;
}

bool FInventorySnapshot::operator!=(const FInventorySnapshot& InOther) const
{
    return !(*this == InOther);
}

bool FInventoryItemFilter::Matches(const FAbilityItem& InItem) const
{
    if (InItem.IsEmpty() || (!bIncludeUnique && InItem.InstanceId.IsValid()))
    {
        return false;
    }
    if (ItemId.IsValid() && ItemId != InItem.ItemId)
    {
        return false;
    }
    if (bMatchLevel && Level != InItem.Level)
    {
        return false;
    }
    return !bMatchSignature || Signature == InItem.Signature();
}

bool FInventoryCommandResult::IsSuccess() const
{
    return Code == EInventoryResultCode::Success || Code == EInventoryResultCode::Partial
        || Code == EInventoryResultCode::AlreadyCommitted;
}

bool InventoryOrder::GuidLess(const FGuid& InLeft, const FGuid& InRight)
{
    if (InLeft.A != InRight.A)
    {
        return InLeft.A < InRight.A;
    }
    if (InLeft.B != InRight.B)
    {
        return InLeft.B < InRight.B;
    }
    if (InLeft.C != InRight.C)
    {
        return InLeft.C < InRight.C;
    }
    return InLeft.D < InRight.D;
}

bool InventoryOrder::SlotKeyLess(const FInventorySlotKey& InLeft, const FInventorySlotKey& InRight)
{
    if (InLeft.Group != InRight.Group)
    {
        return InLeft.Group.ToString().Compare(InRight.Group.ToString(), ESearchCase::IgnoreCase) < 0;
    }
    return InLeft.Index < InRight.Index;
}
