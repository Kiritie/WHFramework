#pragma once

#include "Ability/Inventory/InventoryTypes.h"
#include "UObject/Interface.h"
#include "AbilityInventoryAgentInterface.generated.h"

class UAbilityInventoryBase;

UINTERFACE(MinimalAPI)
class UAbilityInventoryAgentInterface : public UInterface
{
    GENERATED_BODY()
};

class WHFRAMEWORK_API IAbilityInventoryAgentInterface
{
    GENERATED_BODY()

public:
    virtual UAbilityInventoryBase* GetInventory() const = 0;
    virtual FInventorySlotKey GetSelectedInventorySlot() const;
};
