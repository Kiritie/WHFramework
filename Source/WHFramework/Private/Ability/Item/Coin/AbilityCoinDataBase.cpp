#include "Ability/Item/Coin/AbilityCoinDataBase.h"

UAbilityCoinDataBase::UAbilityCoinDataBase()
{
	Type = FName("Coin");
	MaxCount = 999;
	PickUpClass = nullptr;
}
