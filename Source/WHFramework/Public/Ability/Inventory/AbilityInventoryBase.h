#pragma once

#include "Ability/Inventory/InventorySnapshotTypes.h"
#include "Ability/Inventory/InventoryQueryTypes.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AbilityInventoryBase.generated.h"

class UInventoryModule;
class FInventoryCommitCoordinator;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityInventoryBase : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure)
    FGuid GetInventoryId() const;

    uint64 GetRevision() const;

    UFUNCTION(BlueprintPure)
    FInventorySnapshot MakeSnapshot() const;

    const FInventorySlotState* FindSlot(const FInventorySlotKey& InKey) const;
    FInventoryQueryResult Query(const FInventoryQueryRequest& InRequest) const;

private:
    FInventoryState State;
    TWeakObjectPtr<UInventoryModule> OwnerModule;

    friend class UInventoryModule;
    friend class FInventoryCommitCoordinator;
};
