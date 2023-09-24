// Fill out your copyright notice in the Description page of Project Settings.

#include "FSM/Base/FiniteStateBase.h"

#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/FiniteState/EventHandle_EnterFiniteState.h"
#include "Event/Handle/FiniteState/EventHandle_LeaveFiniteState.h"
#include "FSM/Components/FSMComponent.h"
#include "Common/CommonBPLibrary.h"

UFiniteStateBase::UFiniteStateBase()
{
	StateName = FName("FiniteStateBase");
	StateIndex = 0;
	FSM = nullptr;

	const UFunction* OnEnterValidateFunction = GetClass()->FindFunctionByName(FName(TEXT("K2_OnEnterValidate")));
	bHasBlueprintOnEnterValidate = UCommonBPLibrary::IsImplementedInBlueprint(OnEnterValidateFunction);

	const UFunction* OnLeaveValidateFunction = GetClass()->FindFunctionByName(FName(TEXT("K2_OnLeaveValidate")));
	bHasBlueprintOnLeaveValidate = UCommonBPLibrary::IsImplementedInBlueprint(OnLeaveValidateFunction);
}

void UFiniteStateBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	
}

void UFiniteStateBase::OnDespawn_Implementation(bool bRecovery)
{
	StateIndex = 0;
	FSM = nullptr;
}

void UFiniteStateBase::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	FSM = InFSMComponent;
	StateIndex = InStateIndex;
	K2_OnInitialize();
}

bool UFiniteStateBase::OnEnterValidate(UFiniteStateBase* InLastFiniteState)
{
	if(bHasBlueprintOnEnterValidate && !K2_OnEnterValidate(InLastFiniteState)) return false;
	return true;
}

void UFiniteStateBase::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	if(FSM->bShowDebugMessage)
	{
		WHLog(FString::Printf(TEXT("%s=>进入状态: %s"), *GetAgent()->GetActorNameOrLabel(), *StateName.ToString()), EDC_FSM);
	}

	K2_OnEnter(InLastFiniteState);

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_EnterFiniteState::StaticClass(), EEventNetType::Single, this, {this, FSM});
}

void UFiniteStateBase::OnRefresh()
{
	K2_OnRefresh();
}

bool UFiniteStateBase::OnLeaveValidate(UFiniteStateBase* InNextFiniteState)
{
	if(bHasBlueprintOnLeaveValidate && !K2_OnLeaveValidate(InNextFiniteState)) return false;
	return true;
}

void UFiniteStateBase::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	if(FSM->bShowDebugMessage)
	{
		WHLog(FString::Printf(TEXT("%s=>离开状态: %s"), *GetAgent()->GetActorNameOrLabel(), *StateName.ToString()), EDC_FSM);
	}

	K2_OnLeave(InNextFiniteState);

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_LeaveFiniteState::StaticClass(), EEventNetType::Single, this, {this, FSM});
}

void UFiniteStateBase::OnTermination()
{
	K2_OnTermination();
}

void UFiniteStateBase::Terminate()
{
	FSM->TerminateState(this);
}

void UFiniteStateBase::Switch(UFiniteStateBase* InFiniteState)
{
	FSM->SwitchState(InFiniteState);
}

void UFiniteStateBase::SwitchLast()
{
	FSM->SwitchLastState();
}

void UFiniteStateBase::SwitchNext()
{
	FSM->SwitchNextState();
}

bool UFiniteStateBase::IsCurrentState()
{
	return FSM->IsCurrentState(this);
}

AActor* UFiniteStateBase::GetAgent(TSubclassOf<AActor> InAgentClass) const
{
	return FSM->GetAgent(InAgentClass);
}
