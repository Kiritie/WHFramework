// Fill out your copyright notice in the Description page of Project Settings.

#include "FSM/Base/FiniteStateBase.h"

#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/FiniteState/EventHandle_EnterFiniteState.h"
#include "Event/Handle/FiniteState/EventHandle_LeaveFiniteState.h"
#include "FSM/Components/FSMComponent.h"

UFiniteStateBase::UFiniteStateBase()
{
	StateName = FName("FiniteStateBase");
	StateIndex = 0;
	FSMComponent = nullptr;
}

void UFiniteStateBase::OnInitialize(UFSMComponent* InFSMComponent)
{
	FSMComponent = InFSMComponent;
	K2_OnInitialize(InFSMComponent);
}

void UFiniteStateBase::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	WHLog(WH_FSM, Log, TEXT("进入状态: %s"), *StateName.ToString());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("进入状态: %s"), *StateName.ToString()));

	K2_OnEnter(InLastFiniteState);

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_EnterFiniteState::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeObject(this), FParameter::MakeObject(FSMComponent)});
}

void UFiniteStateBase::OnRefresh()
{
	K2_OnRefresh();
}

void UFiniteStateBase::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	WHLog(WH_FSM, Log, TEXT("离开状态: %s"), *StateName.ToString());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, FString::Printf(TEXT("离开状态: %s"), *StateName.ToString()));

	K2_OnLeave(InNextFiniteState);

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_LeaveFiniteState::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeObject(this), FParameter::MakeObject(FSMComponent)});
}

void UFiniteStateBase::OnTerminate()
{
	K2_OnTerminate();
}

void UFiniteStateBase::SwitchLastState()
{
	if(FSMComponent)
	{
		FSMComponent->SwitchLastState();
	}
}

void UFiniteStateBase::SwitchNextState()
{
	if(FSMComponent)
	{
		FSMComponent->SwitchNextState();
	}
}
