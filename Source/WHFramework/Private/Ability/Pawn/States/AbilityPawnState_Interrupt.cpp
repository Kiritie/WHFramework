// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Interrupt.h"

#include "Ability/Pawn/AbilityPawnBase.h"
#include "Ability/Pawn/States/AbilityPawnState_Walk.h"
#include "FSM/Components/FSMComponent.h"

UAbilityPawnState_Interrupt::UAbilityPawnState_Interrupt()
{
	StateName = FName("Interrupt");

	Duration = 0.f;
	RemainTime = 0.f;
}

void UAbilityPawnState_Interrupt::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityPawnState_Interrupt::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	if(!Super::OnPreEnter(InLastState, InParams)) return false;

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	return Pawn->DoAction(GameplayTags::Ability_Pawn_Action_Interrupt);
}

void UAbilityPawnState_Interrupt::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	if(InParams.IsValidIndex(0))
	{
		Duration = InParams[0];
		RemainTime = Duration;
	}

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();
	
	Pawn->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Interrupting);
}

void UAbilityPawnState_Interrupt::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	RemainTime -= GetWorld()->GetDeltaSeconds();
	if (RemainTime <= 0.f)
	{
		FSM->RefreshState();
	}
}

void UAbilityPawnState_Interrupt::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->StopAction(GameplayTags::Ability_Pawn_Action_Interrupt);
		
	Pawn->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Interrupting);
}

void UAbilityPawnState_Interrupt::OnTermination()
{
	Super::OnTermination();
}
