#pragma once

#include "Ability/Inventory/InventoryCommandTypes.h"
#include "Ability/Inventory/InventoryPlanTypes.h"
#include "CoreMinimal.h"
#include "Ability/Inventory/InventoryDomain.h"
#include "Ability/Inventory/InventoryJournal.h"

class UInventoryModule;

class WHFRAMEWORK_API FInventoryCommitCoordinator
{
public:
    explicit FInventoryCommitCoordinator(UInventoryModule& InWorld);
    FInventoryCommandResult Commit(FInventoryPlan InPlan, const FInventoryBatchRequest& InRequest, FString& OutError);
    bool Replay(const FInventoryJournalRecord& InRecord, FString& OutError);

private:
    UInventoryModule& World;

    bool ApplyInventory(const FInventoryJournalChange& InChange, FString& OutError);
};
