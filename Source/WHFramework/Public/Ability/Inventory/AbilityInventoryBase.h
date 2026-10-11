#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Ability/Inventory/InventoryTypes.h"
#include "AbilityInventoryBase.generated.h"

class UInventoryWorldSubsystem;
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
    TWeakObjectPtr<UInventoryWorldSubsystem> OwnerSubsystem;

    friend class UInventoryWorldSubsystem;
    friend class FInventoryCommitCoordinator;
};
