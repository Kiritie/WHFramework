#include "Ability/Inventory/InventoryAbilitySource.h"

#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

bool UInventoryAbilitySource::ResolveItem(FAbilityItem& OutItem) const
{
    OutItem = FAbilityItem();
    const AActor* CurrentAvatar = Avatar.Get();
    const UWorld* World = CurrentAvatar ? CurrentAvatar->GetWorld() : nullptr;
    const UInventoryWorldSubsystem* Subsystem = World ? World->GetSubsystem<UInventoryWorldSubsystem>() : nullptr;
    if (!Subsystem || !Subsystem->IsReady() || !InventoryId.IsValid() || !SlotKey.IsValid())
    {
        return false;
    }
    FInventorySnapshot Snapshot;
    FString Error;
    if (!Subsystem->GetSnapshot(InventoryId, Snapshot, Error))
    {
        return false;
    }
    const FInventorySlotState* Slot = Snapshot.FindSlot(SlotKey);
    if (!Slot || Slot->Item.IsEmpty() || Slot->Item.Signature() != Signature)
    {
        return false;
    }
    OutItem = Slot->Item;
    return true;
}
