// Fill out your copyright notice in the Description page of Project Settings.


#include "FSM/FSMModuleStatics.h"

#include "FSM/FSMModule.h"

void UFSMModuleStatics::RegisterFSM(UFSMComponent* InFSM)
{
	UFSMModule::Get().RegisterFSM(InFSM);
}

void UFSMModuleStatics::UnregisterFSM(UFSMComponent* InFSM)
{
	UFSMModule::Get().UnregisterFSM(InFSM);
}

void UFSMModuleStatics::SwitchGroupStateByIndex(const FName InGroupName, int32 InStateIndex)
{
	UFSMModule::Get().SwitchGroupStateByIndex(InGroupName, InStateIndex);
}

void UFSMModuleStatics::SwitchGroupStateByClass(const FName InGroupName, TSubclassOf<UFiniteStateBase> InStateClass)
{
	UFSMModule::Get().SwitchGroupStateByClass(InGroupName, InStateClass);
}

void UFSMModuleStatics::SwitchGroupDefaultState(const FName InGroupName)
{
	UFSMModule::Get().SwitchGroupDefaultState(InGroupName);
}

void UFSMModuleStatics::SwitchGroupFinalState(const FName InGroupName)
{
	UFSMModule::Get().SwitchGroupFinalState(InGroupName);
}

void UFSMModuleStatics::SwitchGroupLastState(const FName InGroupName)
{
	UFSMModule::Get().SwitchGroupLastState(InGroupName);
}

void UFSMModuleStatics::SwitchGroupNextState(const FName InGroupName)
{
	UFSMModule::Get().SwitchGroupNextState(InGroupName);
}
