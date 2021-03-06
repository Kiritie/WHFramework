// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Death.h"

#include "AbilitySystemComponent.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"

UAbilityCharacterState_Death::UAbilityCharacterState_Death()
{
	StateName = FName("Death");
}

void UAbilityCharacterState_Death::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

bool UAbilityCharacterState_Death::OnEnterValidate(UFiniteStateBase* InLastFiniteState)
{
	return Super::OnEnterValidate(InLastFiniteState);
}

void UAbilityCharacterState_Death::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(Character->GetCharacterData().DyingTag);

	if(Killer)
	{
		Killer->ModifyEXP(Character->GetTotalEXP());
	}

	Character->SetEXP(0);
	Character->SetHealth(0.f);
}

void UAbilityCharacterState_Death::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityCharacterState_Death::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);

	Killer = nullptr;

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(Character->GetCharacterData().DyingTag);
	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(Character->GetCharacterData().DeadTag);
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
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	
	Character->Execute_SetActorVisible(Character, false);
	Character->GetCharacterMovement()->SetActive(false);
	Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(Character->GetCharacterData().DyingTag);
	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(Character->GetCharacterData().DeadTag);

	UObjectPoolModuleBPLibrary::DespawnObject(Character);
}
