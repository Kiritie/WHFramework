// Fill out your copyright notice in the Description page of Project Settings.


#include "FSM/FSMModuleStatics.h"

#include "FSM/FSMModule.h"

void UFSMModuleStatics::RegisterFSM(UFSMComponent* InFSM)
{
	if(UFSMModule* FSMModule = UFSMModule::Get())
	{
		FSMModule->RegisterFSM(InFSM);
	}
}

void UFSMModuleStatics::UnregisterFSM(UFSMComponent* InFSM)
{
	if(UFSMModule* FSMModule = UFSMModule::Get())
	{
		FSMModule->UnregisterFSM(InFSM);
	}
}

void UFSMModuleStatics::SwitchGroupStateByIndex(const FName InGroupName, int32 InStateIndex)
{
	if(UFSMModule* FSMModule = UFSMModule::Get())
	{
		FSMModule->SwitchGroupStateByIndex(InGroupName, InStateIndex);
	}
}

void UFSMModuleStatics::SwitchGroupStateByClass(const FName InGroupName, TSubclassOf<UFiniteStateBase> InStateClass)
{
	if(UFSMModule* FSMModule = UFSMModule::Get())
	{
		FSMModule->SwitchGroupStateByClass(InGroupName, InStateClass);
	}
}

void UFSMModuleStatics::SwitchGroupDefaultState(const FName InGroupName)
{
	if(UFSMModule* FSMModule = UFSMModule::Get())
	{
		FSMModule->SwitchGroupDefaultState(InGroupName);
	}
}

void UFSMModuleStatics::SwitchGroupFinalState(const FName InGroupName)
{
	if(UFSMModule* FSMModule = UFSMModule::Get())
	{
		FSMModule->SwitchGroupFinalState(InGroupName);
	}
}

void UFSMModuleStatics::SwitchGroupLastState(const FName InGroupName)
{
	if(UFSMModule* FSMModule = UFSMModule::Get())
	{
		FSMModule->SwitchGroupLastState(InGroupName);
	}
}

void UFSMModuleStatics::SwitchGroupNextState(const FName InGroupName)
{
	if(UFSMModule* FSMModule = UFSMModule::Get())
	{
		FSMModule->SwitchGroupNextState(InGroupName);
	}
}
