// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Fall.h"

#include "Ability/Character/AbilityCharacterBase.h"

UAbilityCharacterState_Fall::UAbilityCharacterState_Fall()
{
	StateName = FName("Fall");

	MaxPosZ = 0.f;
}

void UAbilityCharacterState_Fall::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Fall::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	if(!Super::OnPreEnter(InLastState, InParams)) return false;

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	return Character->DoAction(GameplayTags::Ability_Character_Action_Fall);
}

void UAbilityCharacterState_Fall::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Character_Falling);
	
	Character->LimitToAnim();

	MaxPosZ = 0.f;
}

void UAbilityCharacterState_Fall::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	MaxPosZ = FMath::Max(Character->GetActorLocation().Z, MaxPosZ);
}

void UAbilityCharacterState_Fall::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	
	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Character_Falling);

	Character->FreeToAnim();

	Character->StopAction(GameplayTags::Ability_Character_Action_Fall);
}

void UAbilityCharacterState_Fall::OnTermination()
{
	Super::OnTermination();
}

float UAbilityCharacterState_Fall::GetFallHeight() const
{
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	return MaxPosZ - Character->GetActorLocation().Z;
}
