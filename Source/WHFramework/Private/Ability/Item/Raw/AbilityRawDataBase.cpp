#include "Ability/Item/Raw/AbilityRawDataBase.h"

UAbilityRawDataBase::UAbilityRawDataBase()
{
	Type = FName("Raw");
	MaxCount = 10;
	MaxLevel = 0;
	RawClass = nullptr;
	PickUpClass = nullptr;
}
