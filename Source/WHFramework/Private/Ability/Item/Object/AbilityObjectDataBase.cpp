#include "Ability/Item/Object/AbilityObjectDataBase.h"

UAbilityObjectDataBase::UAbilityObjectDataBase()
{
	Type = FName("Object");
	MaxCount = 10;
	MaxLevel = 0;
}
