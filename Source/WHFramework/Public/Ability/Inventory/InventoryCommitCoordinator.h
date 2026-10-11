#pragma once

#include "CoreMinimal.h"
#include "Ability/Inventory/InventoryDomain.h"
#include "Ability/Inventory/InventoryJournal.h"

class UInventoryWorldSubsystem;

class WHFRAMEWORK_API FInventoryCommitCoordinator
{
public:
    explicit FInventoryCommitCoordinator(UInventoryWorldSubsystem& InWorld);
    FInventoryCommandResult Commit(FInventoryPlan InPlan, const FInventoryBatchRequest& InRequest, FString& OutError);
    bool Replay(const FInventoryJournalRecord& InRecord, FString& OutError);

private:
    UInventoryWorldSubsystem& World;

    bool ApplyInventory(const FInventoryJournalChange& InChange, FString& OutError);
};
