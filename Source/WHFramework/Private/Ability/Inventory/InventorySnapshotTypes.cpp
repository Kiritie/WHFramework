#include "Ability/Inventory/InventorySnapshotTypes.h"

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

bool InventoryOrder::SlotKeyLess(const FInventorySlotKey& InLeft, const FInventorySlotKey& InRight)
{
    if (InLeft.Group != InRight.Group)
    {
        return InLeft.Group.ToString().Compare(InRight.Group.ToString(), ESearchCase::IgnoreCase) < 0;
    }
    return InLeft.Index < InRight.Index;
}
