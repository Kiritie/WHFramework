// Fill out your copyright notice in the Description page of Project Settings.

#include "FSM/Base/FiniteStateBase.h"

#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleStatics.h"
#include "FSM/Components/FSMComponent.h"
#include "Common/CommonStatics.h"
#include "Event/Handle/FSM/EventHandle_FiniteStateEntered.h"
#include "Event/Handle/FSM/EventHandle_FiniteStateLeaved.h"

UFiniteStateBase::UFiniteStateBase()
{
	StateName = FName("FiniteStateBase");
	StateIndex = 0;
	FSM = nullptr;

	const UFunction* OnPreEnterFunction = GetClass()->FindFunctionByName(FName(TEXT("K2_OnPreEnter")));
	bHasBlueprintOnPreEnter = UCommonStatics::IsImplementedInBlueprint(OnPreEnterFunction);

	const UFunction* OnPreLeaveFunction = GetClass()->FindFunctionByName(FName(TEXT("K2_OnPreLeave")));
	bHasBlueprintOnPreLeave = UCommonStatics::IsImplementedInBlueprint(OnPreLeaveFunction);
}

void UFiniteStateBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	
}

void UFiniteStateBase::OnDespawn_Implementation(bool bRecovery)
{
	StateIndex = 0;
	FSM = nullptr;
}

void UFiniteStateBase::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	FSM = InFSM;
	StateIndex = InStateIndex;
	K2_OnInitialize();
}

bool UFiniteStateBase::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	if(bHasBlueprintOnPreEnter && !K2_OnPreEnter(InLastState, InParams)) return false;
	return true;
}

void UFiniteStateBase::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	if(FSM->bShowDebugMessage)
	{
		WHLog(FString::Printf(TEXT("%s=>进入状态: %s"), *GetAgent()->GetActorNameOrLabel(), *StateName.ToString()), EDC_FSM);
	}

	K2_OnEnter(InLastState, InParams);

	UEventModuleStatics::BroadcastEvent(UEventHandle_FiniteStateEntered::StaticClass(), this, {this, FSM});
}

void UFiniteStateBase::OnRefresh(float DeltaSeconds)
{
	K2_OnRefresh(DeltaSeconds);
}

bool UFiniteStateBase::OnPreLeave(UFiniteStateBase* InNextState)
{
	if(bHasBlueprintOnPreLeave && !K2_OnPreLeave(InNextState)) return false;
	return true;
}

void UFiniteStateBase::OnLeave(UFiniteStateBase* InNextState)
{
	if(FSM->bShowDebugMessage)
	{
		WHLog(FString::Printf(TEXT("%s=>离开状态: %s"), *GetAgent()->GetActorNameOrLabel(), *StateName.ToString()), EDC_FSM);
	}

	K2_OnLeave(InNextState);

	UEventModuleStatics::BroadcastEvent(UEventHandle_FiniteStateLeaved::StaticClass(), this, {this, FSM});
}

void UFiniteStateBase::OnTermination()
{
	K2_OnTermination();
}

void UFiniteStateBase::Terminate()
{
	FSM->TerminateState(this);
}

bool UFiniteStateBase::Switch(UFiniteStateBase* InState, const TArray<FParameter>& InParams)
{
	return FSM->SwitchState(InState, InParams);
}

bool UFiniteStateBase::SwitchLast(const TArray<FParameter>& InParams)
{
	return FSM->SwitchLastState(InParams);
}

bool UFiniteStateBase::SwitchNext(const TArray<FParameter>& InParams)
{
	return FSM->SwitchNextState(InParams);
}

AActor* UFiniteStateBase::GetAgent(TSubclassOf<AActor> InClass) const
{
	return FSM->GetAgent(InClass);
}

bool UFiniteStateBase::IsCurrent()
{
	return FSM->IsCurrentState(this);
}
