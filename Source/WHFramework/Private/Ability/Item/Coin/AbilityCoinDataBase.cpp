#include "Ability/Item/Coin/AbilityCoinDataBase.h"

UAbilityCoinDataBase::UAbilityCoinDataBase()
{
	Type = FName("Coin");
	MaxCount = 999;
	MaxLevel = 0;

	Unit = 1;
}
