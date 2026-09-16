#pragma once
#include "Ability/AbilityModuleTypes.h"
#include "CoreMinimal.h"
class UAbilityInventorySlotBase;
class WHFRAMEWORK_API FVoxelInventoryTransaction
{
public:
	static bool SetSilent(UAbilityInventorySlotBase& Slot, const FAbilityItem& Expected, const FAbilityItem& Next);
	static void RestoreSilent(UAbilityInventorySlotBase& Slot, const FAbilityItem& Previous);
	static void Notify(UAbilityInventorySlotBase& Slot, const FAbilityItem& Previous);
};
