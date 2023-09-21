// Fill out your copyright notice in the Description page of Project Settings.


#include "FSM/FSMModuleBPLibrary.h"

#include "FSM/FSMModule.h"

void UFSMModuleBPLibrary::RegisterFSM(UFSMComponent* InFSM)
{
	if(AFSMModule* FSMModule = AFSMModule::Get())
	{
		FSMModule->RegisterFSM(InFSM);
	}
}

void UFSMModuleBPLibrary::UnregisterFSM(UFSMComponent* InFSM)
{
	if(AFSMModule* FSMModule = AFSMModule::Get())
	{
		FSMModule->UnregisterFSM(InFSM);
	}
}

void UFSMModuleBPLibrary::SwitchGroupStateByIndex(const FName InGroupName, int32 InStateIndex)
{
	if(AFSMModule* FSMModule = AFSMModule::Get())
	{
		FSMModule->SwitchGroupStateByIndex(InGroupName, InStateIndex);
	}
}

void UFSMModuleBPLibrary::SwitchGroupStateByClass(const FName InGroupName, TSubclassOf<UFiniteStateBase> InStateClass)
{
	if(AFSMModule* FSMModule = AFSMModule::Get())
	{
		FSMModule->SwitchGroupStateByClass(InGroupName, InStateClass);
	}
}

void UFSMModuleBPLibrary::SwitchGroupDefaultState(const FName InGroupName)
{
	if(AFSMModule* FSMModule = AFSMModule::Get())
	{
		FSMModule->SwitchGroupDefaultState(InGroupName);
	}
}

void UFSMModuleBPLibrary::SwitchGroupFinalState(const FName InGroupName)
{
	if(AFSMModule* FSMModule = AFSMModule::Get())
	{
		FSMModule->SwitchGroupFinalState(InGroupName);
	}
}

void UFSMModuleBPLibrary::SwitchGroupLastState(const FName InGroupName)
{
	if(AFSMModule* FSMModule = AFSMModule::Get())
	{
		FSMModule->SwitchGroupLastState(InGroupName);
	}
}

void UFSMModuleBPLibrary::SwitchGroupNextState(const FName InGroupName)
{
	if(AFSMModule* FSMModule = AFSMModule::Get())
	{
		FSMModule->SwitchGroupNextState(InGroupName);
	}
}
