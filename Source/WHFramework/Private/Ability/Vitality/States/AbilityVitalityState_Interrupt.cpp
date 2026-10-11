// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityState_Interrupt.h"
#include "Ability/AbilityActionTypes.h"

#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/States/AbilityVitalityState_Walk.h"
#include "FSM/Components/FSMComponent.h"

UAbilityVitalityState_Interrupt::UAbilityVitalityState_Interrupt()
{
	StateName = FName("Interrupt");
	StateDomain = TEXT("Control");

	Duration = 0.35f;
	RemainTime = 0.f;
}

void UAbilityVitalityState_Interrupt::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityVitalityState_Interrupt::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	const AAbilityVitalityBase* Agent = GetAgent<AAbilityVitalityBase>();
	return Agent && Agent->HasAuthority() && !Agent->IsDead() && Super::OnPreEnter(InLastState, InParams);
}

void UAbilityVitalityState_Interrupt::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	const float RequestedDuration = InParams.IsValidIndex(0) ? InParams[0].Get<float>() : 0.35f;
	Duration = FMath::IsFinite(RequestedDuration) && RequestedDuration > 0.f ? FMath::Clamp(RequestedDuration, 0.01f, 600.f) : 0.35f;
	RemainTime = Duration;

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	SetStateTag(GameplayTags::State_Vitality_Interrupting, true);
}

void UAbilityVitalityState_Interrupt::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	RemainTime -= FMath::IsFinite(DeltaSeconds) ? FMath::Max(DeltaSeconds, 0.f) : 0.f;
	if (RemainTime <= 0.f)
	{
		FSM->RefreshState();
	}
}

void UAbilityVitalityState_Interrupt::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();


	SetStateTag(GameplayTags::State_Vitality_Interrupting, false);
}

void UAbilityVitalityState_Interrupt::OnTermination()
{
	Super::OnTermination();
}
