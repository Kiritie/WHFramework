#include "Ability/Effects/ConfiguredAttributeEffect.h"

UConfiguredAttributeEffect::UConfiguredAttributeEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

FName UConfiguredAttributeEffect::GetMagnitudeName(int32 ModifierIndex)
{
	return FName(*FString::Printf(TEXT("ConfiguredAttribute.%d"), ModifierIndex));
}

void UConfiguredAttributeEffect::InitializeAttributes(const TArray<FGameplayAttribute>& Attributes)
{
	Modifiers.Reset();
	TSet<FGameplayAttribute> Seen;
	for (const FGameplayAttribute& Attribute : Attributes)
	{
		if (!Attribute.IsValid() || Seen.Contains(Attribute))
		{
			continue;
		}
		Seen.Add(Attribute);
		for (EGameplayModOp::Type Operation : { EGameplayModOp::Additive, EGameplayModOp::Multiplicitive })
		{
			FSetByCallerFloat Magnitude;
			Magnitude.DataName = GetMagnitudeName(Modifiers.Num());
			FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
			Modifier.Attribute = Attribute;
			Modifier.ModifierOp = Operation;
			Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
		}
	}
}

bool UConfiguredAttributeEffect::ConfigureSpec(FGameplayEffectSpec& Spec,
	const TArray<FAbilityAttributeModifier>& Values, int32 Level) const
{
	if (Spec.Def != this || Level < 1)
	{
		return false;
	}
	TMap<int32, float> Evaluated;
	for (const FAbilityAttributeModifier& Value : Values)
	{
		if (!Value.IsValid(Level))
		{
			return false;
		}
		const EGameplayModOp::Type Operation = Value.Operation == EAbilityAttributeOperation::Add
			? EGameplayModOp::Additive : EGameplayModOp::Multiplicitive;
		const int32 Index = Modifiers.IndexOfByPredicate([&Value, Operation](const FGameplayModifierInfo& Modifier)
		{
			return Modifier.Attribute == Value.Attribute && Modifier.ModifierOp == Operation;
		});
		if (Index == INDEX_NONE || Evaluated.Contains(Index))
		{
			return false;
		}
		Evaluated.Add(Index, Value.Magnitude.Evaluate(Level));
	}
	for (int32 Index = 0; Index < Modifiers.Num(); ++Index)
	{
		const float* Value = Evaluated.Find(Index);
		const float Neutral = Modifiers[Index].ModifierOp == EGameplayModOp::Additive ? 0.f : 1.f;
		Spec.SetSetByCallerMagnitude(GetMagnitudeName(Index), Value ? *Value : Neutral);
	}
	return true;
}
