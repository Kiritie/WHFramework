#pragma once

#include "Ability/Actor/AbilityActorInterface.h"

#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Attributes/AttributeSetBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"

void IAbilityActorInterface::InitializeAbilitySystem(AActor* InOwnerActor, AActor* InAvatarActor)
{
	UAbilitySystemComponentBase* AbilitySystemComponent = Cast<UAbilitySystemComponentBase>(GetAbilitySystemComponent());
	UAttributeSetBase* AttributeSet = GetAttributeSet();

	AbilitySystemComponent->InitAbilityActorInfo(InOwnerActor ? InOwnerActor : Cast<AActor>(this), InAvatarActor ? InAvatarActor : Cast<AActor>(this));
	RefreshAttributes();

	for(auto Iter : AttributeSet->GetAllAttributes())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Iter).AddRaw(this, &IAbilityActorInterface::OnAttributeChange);
	}
}

void IAbilityActorInterface::RefreshAttributes()
{
	UAttributeSetBase* AttributeSet = GetAttributeSet();
	for(auto Iter : AttributeSet->GetAllAttributes())
	{
		FOnAttributeChangeData OnAttributeChangeData;
		OnAttributeChangeData.Attribute = Iter;
		OnAttributeChangeData.OldValue = AttributeSet->GetAttributeValue(Iter);
		OnAttributeChangeData.NewValue = AttributeSet->GetAttributeValue(Iter);
		OnAttributeChange(OnAttributeChangeData);
	}
}
