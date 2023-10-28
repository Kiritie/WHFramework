// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Death.h"

#include "AbilitySystemComponent.h"
#include "Ability/Pawn/AbilityPawnBase.h"
#include "Ability/Pawn/AbilityPawnDataBase.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"

UAbilityPawnState_Death::UAbilityPawnState_Death()
{
	StateName = FName("Death");
}

void UAbilityPawnState_Death::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

bool UAbilityPawnState_Death::OnEnterValidate(UFiniteStateBase* InLastFiniteState)
{
	return Super::OnEnterValidate(InLastFiniteState);
}

void UAbilityPawnState_Death::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::StateTag_Vitality_Dying);

	if(Killer)
	{
		Killer->ModifyExp(Pawn->GetLevelV() * 5.f);
	}

	Pawn->SetExp(0);
	Pawn->SetHealth(0.f);

	DeathStart();
}

void UAbilityPawnState_Death::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityPawnState_Death::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);

	Killer = nullptr;

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Vitality_Dying);
	Pawn->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Vitality_Dead);
}

void UAbilityPawnState_Death::OnTermination()
{
	Super::OnTermination();
}

void UAbilityPawnState_Death::DeathStart()
{
	DeathEnd();
}

void UAbilityPawnState_Death::DeathEnd()
{
	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();
	
	Pawn->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Vitality_Dying);
	
	Pawn->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::StateTag_Vitality_Dead);

	UObjectPoolModuleBPLibrary::DespawnObject(Pawn);
}
