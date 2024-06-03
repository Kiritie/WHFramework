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

bool UFSMModuleStatics::HasFSMGroup(const FName InGroupName)
{
	return UFSMModule::Get().HasGroup(InGroupName);
}

FFSMGroup UFSMModuleStatics::GetFSMGroup(const FName InGroupName)
{
	return UFSMModule::Get().GetGroup(InGroupName);
}

void UFSMModuleStatics::SwitchFSMGroupStateByIndex(const FName InGroupName, int32 InStateIndex, const TArray<FParameter>& InParams)
{
	UFSMModule::Get().SwitchGroupStateByIndex(InGroupName, InStateIndex, InParams);
}

void UFSMModuleStatics::SwitchFSMGroupStateByName(const FName InGroupName, const FName InStateName, const TArray<FParameter>& InParams)
{
	UFSMModule::Get().SwitchGroupStateByName(InGroupName, InStateName, InParams);
}

void UFSMModuleStatics::SwitchFSMGroupStateByClass(const FName InGroupName, TSubclassOf<UFiniteStateBase> InStateClass, const TArray<FParameter>& InParams)
{
	UFSMModule::Get().SwitchGroupStateByClass(InGroupName, InStateClass, InParams);
}

void UFSMModuleStatics::SwitchFSMGroupDefaultState(const FName InGroupName, const TArray<FParameter>& InParams)
{
	UFSMModule::Get().SwitchGroupDefaultState(InGroupName, InParams);
}

void UFSMModuleStatics::SwitchFSMGroupFinalState(const FName InGroupName, const TArray<FParameter>& InParams)
{
	UFSMModule::Get().SwitchGroupFinalState(InGroupName, InParams);
}

void UFSMModuleStatics::SwitchFSMGroupLastState(const FName InGroupName, const TArray<FParameter>& InParams)
{
	UFSMModule::Get().SwitchGroupLastState(InGroupName, InParams);
}

void UFSMModuleStatics::SwitchFSMGroupNextState(const FName InGroupName, const TArray<FParameter>& InParams)
{
	UFSMModule::Get().SwitchGroupNextState(InGroupName, InParams);
}
