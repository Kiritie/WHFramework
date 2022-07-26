#pragma once

#include "Ability/Actor/AbilityActorInterface.h"

#include "Ability/Attributes/AttributeSetBase.h"

void IAbilityActorInterface::InitializeAbilitySystem()
{
	GetAbilitySystemComponent()->InitAbilityActorInfo(Cast<AActor>(this), Cast<AActor>(this));
	RefreshAttributes();
}

void IAbilityActorInterface::RefreshAttributes()
{
	UAttributeSetBase* AttributeSet = GetAttributeSet();
	for(auto Iter : AttributeSet->GetAllAttributes())
	{
		FOnAttributeChangeData OnAttributeChangeData;
		OnAttributeChangeData.Attribute = Iter;
		OnAttributeChangeData.OldValue = GetAttributeValue(Iter);
		OnAttributeChangeData.NewValue = GetAttributeValue(Iter);
		OnAttributeChange(OnAttributeChangeData);
	}
}
