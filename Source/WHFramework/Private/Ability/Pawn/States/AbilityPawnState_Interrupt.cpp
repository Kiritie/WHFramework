// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Interrupt.h"
#include "Ability/AbilityActionTypes.h"

#include "Ability/Pawn/AbilityPawnBase.h"
#include "Ability/Pawn/States/AbilityPawnState_Walk.h"
#include "FSM/Components/FSMComponent.h"
#include "GameFramework/PawnMovementComponent.h"

UAbilityPawnState_Interrupt::UAbilityPawnState_Interrupt()
{
	StateName = FName("Interrupt");
	StateDomain = TEXT("Control");

	Duration = 0.35f;
	RemainTime = 0.f;
}

void UAbilityPawnState_Interrupt::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityPawnState_Interrupt::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	const AAbilityPawnBase* Agent = GetAgent<AAbilityPawnBase>();
	return Agent && Agent->HasAuthority() && !Agent->IsDead() && Super::OnPreEnter(InLastState, InParams);
}

void UAbilityPawnState_Interrupt::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	const float RequestedDuration = InParams.IsValidIndex(0) ? InParams[0].Get<float>() : 0.35f;
	Duration = FMath::IsFinite(RequestedDuration) && RequestedDuration > 0.f ? FMath::Clamp(RequestedDuration, 0.01f, 600.f) : 0.35f;
	RemainTime = Duration;

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	SetStateTag(GameplayTags::State_Vitality_Interrupting, true);

	if (Pawn->GetMovementComponent()) { Pawn->GetMovementComponent()->StopMovementImmediately(); Pawn->GetMovementComponent()->SetActive(false); }
}

void UAbilityPawnState_Interrupt::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	RemainTime -= FMath::IsFinite(DeltaSeconds) ? FMath::Max(DeltaSeconds, 0.f) : 0.f;
	if (RemainTime <= 0.f)
	{
		FSM->RefreshState();
	}
}

void UAbilityPawnState_Interrupt::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();


	SetStateTag(GameplayTags::State_Vitality_Interrupting, false);

	if (Pawn->GetMovementComponent() && FSM && !FSM->IsTerminating() && !InNextState && Pawn->IsActive() && !Pawn->IsDead()) { Pawn->GetMovementComponent()->SetActive(true); }
}

void UAbilityPawnState_Interrupt::OnTermination()
{
	Super::OnTermination();
}
