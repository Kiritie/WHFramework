#include "Ability/Item/Equip/AbilityEquipDataBase.h"
#include "Ability/AbilityItemTypes.h"

UAbilityEquipDataBase::UAbilityEquipDataBase()
{
	Type = FName("Equip");
	MaxCount = 1;
	EquipMode = EAbilityEquipMode::None;
	EquipClass = nullptr;
}
