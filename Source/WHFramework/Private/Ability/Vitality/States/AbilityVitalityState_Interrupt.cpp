// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityState_Interrupt.h"

#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/States/AbilityVitalityState_Walk.h"
#include "FSM/Components/FSMComponent.h"

UAbilityVitalityState_Interrupt::UAbilityVitalityState_Interrupt()
{
	StateName = FName("Interrupt");

	Duration = 0.f;
	RemainTime = 0.f;
}

void UAbilityVitalityState_Interrupt::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityVitalityState_Interrupt::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	if(!Super::OnPreEnter(InLastState, InParams)) return false;

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	return Vitality->DoAction(GameplayTags::Ability_Vitality_Action_Interrupt);
}

void UAbilityVitalityState_Interrupt::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	if(InParams.IsValidIndex(0))
	{
		Duration = InParams[0];
		RemainTime = Duration;
	}

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();
	
	Vitality->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Interrupting);
}

void UAbilityVitalityState_Interrupt::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	RemainTime -= GetWorld()->GetDeltaSeconds();
	if (RemainTime <= 0.f)
	{
		FSM->RefreshState();
	}
}

void UAbilityVitalityState_Interrupt::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	Vitality->StopAction(GameplayTags::Ability_Vitality_Action_Interrupt);
		
	Vitality->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Interrupting);
}

void UAbilityVitalityState_Interrupt::OnTermination()
{
	Super::OnTermination();
}
