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

	const UFunction* OnEnterValidateFunction = GetClass()->FindFunctionByName(FName(TEXT("K2_OnEnterValidate")));
	bHasBlueprintOnEnterValidate = UCommonStatics::IsImplementedInBlueprint(OnEnterValidateFunction);

	const UFunction* OnLeaveValidateFunction = GetClass()->FindFunctionByName(FName(TEXT("K2_OnLeaveValidate")));
	bHasBlueprintOnLeaveValidate = UCommonStatics::IsImplementedInBlueprint(OnLeaveValidateFunction);
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

bool UFiniteStateBase::OnEnterValidate(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	if(bHasBlueprintOnEnterValidate && !K2_OnEnterValidate(InLastState, InParams)) return false;
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

void UFiniteStateBase::OnRefresh()
{
	K2_OnRefresh();
}

bool UFiniteStateBase::OnLeaveValidate(UFiniteStateBase* InNextState)
{
	if(bHasBlueprintOnLeaveValidate && !K2_OnLeaveValidate(InNextState)) return false;
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
