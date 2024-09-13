#include "Ability/Item/Raw/AbilityRawDataBase.h"

UAbilityRawDataBase::UAbilityRawDataBase()
{
	Type = FName("Raw");
	MaxCount = 10;
	RawClass = nullptr;
	PickUpClass = nullptr;
}
