#pragma once

#include "Ability/Actor/AbilityActorInterface.h"

#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Attributes/AttributeSetBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"

void IAbilityActorInterface::InitializeAbilities(AActor* InOwnerActor, AActor* InAvatarActor)
{
	if(bAbilityInitialized) return;

	UAbilitySystemComponentBase* AbilitySystemComponent = Cast<UAbilitySystemComponentBase>(GetAbilitySystemComponent());
	UAttributeSetBase* AttributeSet = GetAttributeSet();

	InOwnerActor = InOwnerActor ? InOwnerActor : Cast<AActor>(this);
	if(AbilitySystemComponent->HasBeenInitialized())
	{
		AbilitySystemComponent->InitAbilityActorInfo(InOwnerActor, InOwnerActor);
	}

	for(auto& Iter : AttributeSet->GetAllAttributes())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Iter).AddRaw(this, &IAbilityActorInterface::OnAttributeChange);
	}

	bAbilityInitialized = true;
}

void IAbilityActorInterface::RefreshAttributes()
{
	UAttributeSetBase* AttributeSet = GetAttributeSet();
	for(auto& Iter : AttributeSet->GetAllAttributes())
	{
		FOnAttributeChangeData OnAttributeChangeData;
		OnAttributeChangeData.Attribute = Iter;
		OnAttributeChangeData.OldValue = AttributeSet->GetAttributeValue(Iter);
		OnAttributeChangeData.NewValue = AttributeSet->GetAttributeValue(Iter);
		OnAttributeChange(OnAttributeChangeData);
	}
}
