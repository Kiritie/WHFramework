#include "Ability/AbilityLevelValue.h"

#include <limits>

float FAbilityLevelValue::Evaluate(int32 InLevel) const
{
	const int32 Steps = FMath::Max(1, InLevel) - 1;
	switch (Growth)
	{
		case EAbilityValueGrowth::None: return Base;
		case EAbilityValueGrowth::Linear: return Base + Increment * Steps;
		case EAbilityValueGrowth::Exponential: return Base * FMath::Pow(Multiplier, Steps);
		default: return std::numeric_limits<float>::quiet_NaN();
	}
}

bool FAbilityLevelValue::IsValid(int32 InMaxLevel) const
{
	if (InMaxLevel < 1 || !FMath::IsFinite(Base)) return false;
	switch (Growth)
	{
		case EAbilityValueGrowth::None: return true;
		case EAbilityValueGrowth::Linear: return FMath::IsFinite(Increment) && FMath::IsFinite(Evaluate(InMaxLevel));
		case EAbilityValueGrowth::Exponential: return FMath::IsFinite(Multiplier) && Multiplier > 0.f && FMath::IsFinite(Evaluate(InMaxLevel));
		default: return false;
	}
}
