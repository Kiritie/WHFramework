// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityState_Death.h"

#include "AbilitySystemComponent.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/AbilityVitalityInventoryBase.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Ability/EventHandle_VitalityDead.h"

UAbilityVitalityState_Death::UAbilityVitalityState_Death()
{
	StateName = FName("Death");

	Killer = nullptr;
}

void UAbilityVitalityState_Death::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityVitalityState_Death::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityVitalityState_Death::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	if(InParams.IsValidIndex(0))
	{
		Killer = InParams[0].GetPointerValue<IAbilityVitalityInterface>();
	}
	
	UEventModuleStatics::BroadcastEvent<UEventHandle_VitalityDead>(this, { GetAgent(), Cast<UObject>(Killer) });

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	Vitality->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Active);
	Vitality->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Dying);

	Vitality->GetInteractionComponent()->SetInteractable(false);

	Vitality->SetExp(0.f);
	Vitality->SetHealth(0.f);

	DeathStart();
}

void UAbilityVitalityState_Death::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

bool UAbilityVitalityState_Death::OnPreLeave(UFiniteStateBase* InNextState)
{
	if(!Super::OnPreLeave(InNextState)) return false;

	return InNextState && InNextState->IsA<UAbilityVitalityState_Spawn>();
}

void UAbilityVitalityState_Death::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	Killer = nullptr;

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	Vitality->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Dying);
	Vitality->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Dead);
	Vitality->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Active);

	Vitality->StopAction(GameplayTags::Ability_Vitality_Action_Death);
}

void UAbilityVitalityState_Death::OnTermination()
{
	Super::OnTermination();
}

void UAbilityVitalityState_Death::DeathStart()
{
	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	Vitality->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if(!Vitality->DoAction(GameplayTags::Ability_Vitality_Action_Death))
	{
		DeathEnd();
	}
}

void UAbilityVitalityState_Death::DeathEnd()
{
	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();
	
	Vitality->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Dying);
	Vitality->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Dead);

	Vitality->Inventory->DiscardItems();

	UObjectPoolModuleStatics::DespawnObject(Vitality);
}
