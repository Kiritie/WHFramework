// Fill out your copyright notice in the Description page of Project Settings.


#include "FSM/FSMModule.h"

#include "FSM/FSMModuleNetworkComponent.h"
#include "FSM/Components/FSMComponent.h"
		
IMPLEMENTATION_MODULE(UFSMModule)

// ParamSets default values
UFSMModule::UFSMModule()
{
	ModuleName = FName("FSMModule");
	ModuleDisplayName = FText::FromString(TEXT("FSM Module"));

	ModuleNetworkComponent = UFSMModuleNetworkComponent::StaticClass();

	GroupMap = TMap<FName, FFSMGroupInfo>();
}

UFSMModule::~UFSMModule()
{
	TERMINATION_MODULE(UFSMModule)
}

#if WITH_EDITOR
void UFSMModule::OnGenerate()
{
	Super::OnGenerate();
}

void UFSMModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UFSMModule)
}
#endif

void UFSMModule::OnInitialize()
{
	Super::OnInitialize();
}

void UFSMModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UFSMModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UFSMModule::OnPause()
{
	Super::OnPause();
}

void UFSMModule::OnUnPause()
{
	Super::OnUnPause();
}

void UFSMModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

void UFSMModule::RegisterFSM(UFSMComponent* InFSM)
{
	if(!HasGroup(InFSM->GroupName)) GroupMap.Add(InFSM->GroupName);

	if(!GetGroups(InFSM->GroupName).Contains(InFSM))
	{
		GetGroups(InFSM->GroupName).Add(InFSM);
	}
}

void UFSMModule::UnregisterFSM(UFSMComponent* InFSM)
{
	if(!HasGroup(InFSM->GroupName)) return;

	if(GetGroups(InFSM->GroupName).Contains(InFSM))
	{
		GetGroups(InFSM->GroupName).Remove(InFSM);
	}
}

void UFSMModule::SwitchGroupStateByIndex(const FName InGroupName, int32 InStateIndex)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroups(InGroupName))
	{
		Iter->SwitchStateByIndex(InStateIndex);
	}
}

void UFSMModule::SwitchGroupStateByClass(const FName InGroupName, TSubclassOf<UFiniteStateBase> InStateClass)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroups(InGroupName))
	{
		Iter->SwitchStateByClass(InStateClass);
	}
}

void UFSMModule::SwitchGroupDefaultState(const FName InGroupName)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroups(InGroupName))
	{
		Iter->SwitchDefaultState();
	}
}

void UFSMModule::SwitchGroupFinalState(const FName InGroupName)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroups(InGroupName))
	{
		Iter->SwitchFinalState();
	}
}

void UFSMModule::SwitchGroupLastState(const FName InGroupName)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroups(InGroupName))
	{
		Iter->SwitchLastState();
	}
}

void UFSMModule::SwitchGroupNextState(const FName InGroupName)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroups(InGroupName))
	{
		Iter->SwitchNextState();
	}
}
