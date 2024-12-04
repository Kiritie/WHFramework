// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Death.h"

#include "AbilitySystemComponent.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/AbilityCharacterInventoryBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "AI/Base/AIControllerBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Common/Looking/LookingComponent.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Ability/EventHandle_VitalityDead.h"

UAbilityCharacterState_Death::UAbilityCharacterState_Death()
{
	StateName = FName("Death");

	Killer = nullptr;
	bDeathStarted = false;
}

void UAbilityCharacterState_Death::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Death::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityCharacterState_Death::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	if(InParams.IsValidIndex(0))
	{
		Killer = InParams[0].GetPointerValue<IAbilityVitalityInterface>();
	}
	
	UEventModuleStatics::BroadcastEvent<UEventHandle_VitalityDead>(Cast<UObject>(this), { GetAgent(), Cast<UObject>(Killer) });

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Dying);

	Character->GetInteractionComponent()->SetInteractable(false);

	if(Character->GetController<AAIControllerBase>())
	{
		Character->GetController<AAIControllerBase>()->StopBehaviorTree();
	}

	if(Killer && Killer != Character)
	{
		Killer->ModifyExp(Character->GetLevelA() * 10.f);
	}

	Character->SetExp(0.f);
	Character->SetHealth(0.f);

	Character->GetLooking()->TargetLookingOff();
	Character->LimitToAnim();
}

void UAbilityCharacterState_Death::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	if (Character->IsDying() && !Character->IsFalling(true) && !bDeathStarted)
	{
		DeathStart();
	}
}

bool UAbilityCharacterState_Death::OnPreLeave(UFiniteStateBase* InNextState)
{
	if(!Super::OnPreLeave(InNextState)) return false;

	return InNextState && InNextState->IsA<UAbilityCharacterState_Spawn>();
}

void UAbilityCharacterState_Death::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	Killer = nullptr;
	bDeathStarted = false;

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Dying);
	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Dead);

	Character->StopAction(GameplayTags::Ability_Vitality_Action_Death);

	if(Character->IsPlayer())
	{
		Character->Execute_SetActorVisible(Character, true);
	}
}

void UAbilityCharacterState_Death::OnTermination()
{
	Super::OnTermination();
}

void UAbilityCharacterState_Death::DeathStart()
{
	bDeathStarted = true;

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetCharacterMovement()->SetActive(false);
	Character->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if(!Character->DoAction(GameplayTags::Ability_Vitality_Action_Death))
	{
		DeathEnd();
	}
}

void UAbilityCharacterState_Death::DeathEnd()
{
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	
	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Dying);
	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Dead);

	Character->Inventory->DiscardItems();

	if(!Character->IsPlayer())
	{
		UObjectPoolModuleStatics::DespawnObject(Character);
	}
	else
	{
		Character->Execute_SetActorVisible(Character, false);
	}
}
