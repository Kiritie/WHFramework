#include "Ability/Item/Prop/AbilityPropDataBase.h"

UAbilityPropDataBase::UAbilityPropDataBase()
{
	Type = FName("Prop");
	MaxCount = 10;
	PropClass = nullptr;
	PropPickUpClass = nullptr;
}
