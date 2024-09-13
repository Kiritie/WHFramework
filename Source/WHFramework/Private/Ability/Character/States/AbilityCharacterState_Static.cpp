// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Static.h"

#include "AbilitySystemComponent.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UAbilityCharacterState_Static::UAbilityCharacterState_Static()
{
	StateName = FName("Static");
}

void UAbilityCharacterState_Static::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Static::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityCharacterState_Static::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Character_Active);

	Character->GetCharacterMovement()->SetActive(false);
	Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Character->GetInteractionComponent()->SetInteractable(false);
}

void UAbilityCharacterState_Static::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityCharacterState_Static::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::StateTag_Character_Active);

	Character->GetCharacterMovement()->SetActive(true);
	Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Character->GetInteractionComponent()->SetInteractable(true);
}

void UAbilityCharacterState_Static::OnTermination()
{
	Super::OnTermination();
}
