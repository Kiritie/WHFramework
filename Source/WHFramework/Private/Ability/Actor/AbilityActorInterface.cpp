#pragma once

#include "Ability/Actor/AbilityActorInterface.h"

#include "Ability/Attributes/AttributeSetBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"

void IAbilityActorInterface::InitializeAbilities(AActor* InOwnerActor, AActor* InAvatarActor)
{
	if(bAbilitiesInitialized) return;

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

	bAbilitiesInitialized = true;
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

bool IAbilityActorInterface::AttachActor(AActor* InActor, const FAttachmentTransformRules& InAttachmentRules, FName InSocketName)
{
	if(InActor->AttachToComponent(GetMeshComponent(), InAttachmentRules, InSocketName))
	{
		OnActorAttached(InActor);
		return true;
	}
	return false;
}

void IAbilityActorInterface::DetachActor(AActor* InActor, const FDetachmentTransformRules& InDetachmentRules)
{
	InActor->DetachFromActor(InDetachmentRules);
	OnActorDetached(InActor);
}
