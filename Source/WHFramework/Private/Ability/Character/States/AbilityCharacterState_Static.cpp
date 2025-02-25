// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Static.h"

#include "AbilitySystemComponent.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "AI/Base/AIControllerBase.h"
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

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Active);

	Character->DoAction(GameplayTags::Ability_Vitality_Action_Static);

	Character->LimitToAnim();

	Character->GetCharacterMovement()->SetActive(false);

	if(Character->GetController<AAIControllerBase>())
	{
		Character->GetController<AAIControllerBase>()->StopBehaviorTree();
	}
}

void UAbilityCharacterState_Static::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityCharacterState_Static::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Active);

	Character->StopAction(GameplayTags::Ability_Vitality_Action_Static);

	Character->FreeToAnim();

	Character->GetCharacterMovement()->SetActive(true);

	if(Character->GetController<AAIControllerBase>())
	{
		Character->GetController<AAIControllerBase>()->RunBehaviorTree();
	}
}

void UAbilityCharacterState_Static::OnTermination()
{
	Super::OnTermination();
}
