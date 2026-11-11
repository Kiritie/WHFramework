#include "Ability/AbilityAttributeModifier.h"

bool FAbilityAttributeModifier::IsValid(int32 MaxLevel) const
{
	if (!Attribute.IsValid() || !Magnitude.IsValid(MaxLevel))
	{
		return false;
	}
	switch (Operation)
	{
		case EAbilityAttributeOperation::Add:
			return true;
		case EAbilityAttributeOperation::Multiply:
			return Magnitude.Evaluate(1) >= 0.f && Magnitude.Evaluate(MaxLevel) >= 0.f;
		default:
			return false;
	}
}

bool FAbilityAttributeModifier::ValidateList(const TArray<FAbilityAttributeModifier>& Values, int32 MaxLevel, FString& OutError)
{
	OutError.Reset();
	if (MaxLevel < 1 || MaxLevel > 100000 || Values.Num() > 128)
	{
		OutError = TEXT("属性配置的最大等级或数量无效。");
		return false;
	}
	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		const FAbilityAttributeModifier& Value = Values[Index];
		if (!Value.IsValid(MaxLevel))
		{
			OutError = FString::Printf(TEXT("第 %d 项属性或成长公式无效。"), Index + 1);
			return false;
		}
		for (int32 Previous = 0; Previous < Index; ++Previous)
		{
			if (Values[Previous].Attribute == Value.Attribute && Values[Previous].Operation == Value.Operation)
			{
				OutError = TEXT("同一属性的同一种运算只能配置一次。");
				return false;
			}
		}
	}
	return true;
}
