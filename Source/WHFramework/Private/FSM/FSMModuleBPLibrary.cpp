// Fill out your copyright notice in the Description page of Project Settings.


#include "FSM/FSMModuleBPLibrary.h"

void UFSMModuleBPLibrary::RegisterFSM(UFSMComponent* InFSM)
{
	if(AFSMModule* FSMModule = AMainModule::GetModuleByClass<AFSMModule>())
	{
		FSMModule->RegisterFSM(InFSM);
	}
}

void UFSMModuleBPLibrary::UnregisterFSM(UFSMComponent* InFSM)
{
	if(AFSMModule* FSMModule = AMainModule::GetModuleByClass<AFSMModule>())
	{
		FSMModule->UnregisterFSM(InFSM);
	}
}

void UFSMModuleBPLibrary::SwitchFSMGroupStateByIndex(const FName InFSMGroup, int32 InStateIndex)
{
	if(AFSMModule* FSMModule = AMainModule::GetModuleByClass<AFSMModule>())
	{
		FSMModule->SwitchFSMGroupStateByIndex(InFSMGroup, InStateIndex);
	}
}

void UFSMModuleBPLibrary::SwitchFSMGroupStateByClass(const FName InFSMGroup, TSubclassOf<UFiniteStateBase> InStateClass)
{
	if(AFSMModule* FSMModule = AMainModule::GetModuleByClass<AFSMModule>())
	{
		FSMModule->SwitchFSMGroupStateByClass(InFSMGroup, InStateClass);
	}
}

void UFSMModuleBPLibrary::SwitchFSMGroupDefaultState(const FName InFSMGroup)
{
	if(AFSMModule* FSMModule = AMainModule::GetModuleByClass<AFSMModule>())
	{
		FSMModule->SwitchFSMGroupDefaultState(InFSMGroup);
	}
}

void UFSMModuleBPLibrary::SwitchFSMGroupFinalState(const FName InFSMGroup)
{
	if(AFSMModule* FSMModule = AMainModule::GetModuleByClass<AFSMModule>())
	{
		FSMModule->SwitchFSMGroupFinalState(InFSMGroup);
	}
}

void UFSMModuleBPLibrary::SwitchFSMGroupLastState(const FName InFSMGroup)
{
	if(AFSMModule* FSMModule = AMainModule::GetModuleByClass<AFSMModule>())
	{
		FSMModule->SwitchFSMGroupLastState(InFSMGroup);
	}
}

void UFSMModuleBPLibrary::SwitchFSMGroupNextState(const FName InFSMGroup)
{
	if(AFSMModule* FSMModule = AMainModule::GetModuleByClass<AFSMModule>())
	{
		FSMModule->SwitchFSMGroupNextState(InFSMGroup);
	}
}

bool UFSMModuleBPLibrary::HasFSMGroup(const FName InFSMGroup)
{
	if(AFSMModule* FSMModule = AMainModule::GetModuleByClass<AFSMModule>())
	{
		return FSMModule->HasFSMGroup(InFSMGroup);
	}
	return false;
}

TArray<UFSMComponent*> UFSMModuleBPLibrary::GetFSMGroupArray(const FName InFSMGroup)
{
	if(AFSMModule* FSMModule = AMainModule::GetModuleByClass<AFSMModule>())
	{
		return FSMModule->GetFSMGroupArray(InFSMGroup);
	}
	return TArray<UFSMComponent*>();
}
