#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Ability/Inventory/InventoryTypes.h"
#include "InventoryAbilitySource.generated.h"

class AActor;

UCLASS()
class WHFRAMEWORK_API UInventoryAbilitySource : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(Transient)
    FGuid InventoryId;

    UPROPERTY(Transient)
    FInventorySlotKey SlotKey;

    UPROPERTY(Transient)
    FInventoryStackSignature Signature;

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> Avatar;

    bool ResolveItem(FAbilityItem& OutItem) const;
};
