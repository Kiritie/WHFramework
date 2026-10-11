#include "Ability/Effects/Calculation/UEffectCalculation_Constant.h"

float UUEffectCalculation_Constant::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	return 1.f;
}
