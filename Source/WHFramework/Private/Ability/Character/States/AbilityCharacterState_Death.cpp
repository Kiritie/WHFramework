// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Death.h"

#include "AbilitySystemComponent.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "AI/Base/AIControllerBase.h"
#include "Common/Interaction/InteractionComponent.h"

UAbilityCharacterState_Death::UAbilityCharacterState_Death()
{
	StateName = FName("Death");
	bDeathStarted = false;
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

	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::StateTag_Vitality_Dying);

	if(Character->GetController<AAIControllerBase>())
	{
		Character->GetController<AAIControllerBase>()->StopBehaviorTree();
	}

	if(Killer)
	{
		Killer->ModifyExp(Character->GetLevelV() * 10.f);
	}

	Character->SetExp(0);
	Character->SetHealth(0.f);
}

void UAbilityCharacterState_Death::OnRefresh()
{
	Super::OnRefresh();

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	if (Character->IsDying() && !Character->IsFalling(true) && !bDeathStarted)
	{
		DeathStart();
	}
}

void UAbilityCharacterState_Death::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);

	Killer = nullptr;
	bDeathStarted = false;

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Vitality_Dying);
	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Vitality_Dead);

	Character->GetCharacterMovement()->SetActive(true);
	Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Character->GetInteractionComponent()->SetGenerateOverlapEvents(true);
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
	Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Character->GetInteractionComponent()->SetGenerateOverlapEvents(false);
}

void UAbilityCharacterState_Death::DeathEnd()
{
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	
	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Vitality_Dying);
	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::StateTag_Vitality_Dead);

	UObjectPoolModuleStatics::DespawnObject(Character);
}
