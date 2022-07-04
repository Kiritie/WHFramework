// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Death.h"

#include "AbilitySystemComponent.h"
#include "Ability/Character/AbilityCharacterBase.h"

UAbilityCharacterState_Death::UAbilityCharacterState_Death()
{
	StateName = FName("Death");
}

void UAbilityCharacterState_Death::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

void UAbilityCharacterState_Death::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);

	GetAgent<AAbilityCharacterBase>()->GetAbilitySystemComponent()
	->AddLooseGameplayTag(GetAgent<AAbilityCharacterBase>()->GetCharacterData().DyingTag);

	DeathStart();
}

void UAbilityCharacterState_Death::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityCharacterState_Death::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);

	GetAgent<AAbilityCharacterBase>()->GetAbilitySystemComponent()
	->RemoveLooseGameplayTag(GetAgent<AAbilityCharacterBase>()->GetCharacterData().DyingTag);

	GetAgent<AAbilityCharacterBase>()->GetAbilitySystemComponent()
	->RemoveLooseGameplayTag(GetAgent<AAbilityCharacterBase>()->GetCharacterData().DeadTag);
}

void UAbilityCharacterState_Death::OnTermination()
{
	Super::OnTermination();
}

void UAbilityCharacterState_Death::DeathStart()
{
	DeathEnd();
}

void UAbilityCharacterState_Death::DeathEnd()
{
	GetAgent<AAbilityCharacterBase>()->GetAbilitySystemComponent()
	->RemoveLooseGameplayTag(GetAgent<AAbilityCharacterBase>()->GetCharacterData().DyingTag);
	
	GetAgent<AAbilityCharacterBase>()->GetAbilitySystemComponent()
	->AddLooseGameplayTag(GetAgent<AAbilityCharacterBase>()->GetCharacterData().DeadTag);
}
