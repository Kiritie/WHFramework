// Fill out your copyright notice in the Description page of Project Settings.


#include "FSM/FSMModule.h"

#include "FSM/FSMModuleNetworkComponent.h"
#include "FSM/Components/FSMComponent.h"
		
IMPLEMENTATION_MODULE(AFSMModule)

// ParamSets default values
AFSMModule::AFSMModule()
{
	ModuleName = FName("FSMModule");

	ModuleNetworkComponent = UFSMModuleNetworkComponent::StaticClass();

	GroupMap = TMap<FName, FFSMGroupInfo>();
}

AFSMModule::~AFSMModule()
{
	TERMINATION_MODULE(AFSMModule)
}

#if WITH_EDITOR
void AFSMModule::OnGenerate()
{
	Super::OnGenerate();
}

void AFSMModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AFSMModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AFSMModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
}

void AFSMModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AFSMModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AFSMModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AFSMModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);
}

void AFSMModule::RegisterFSM(UFSMComponent* InFSM)
{
	if(!HasGroup(InFSM->GroupName)) GroupMap.Add(InFSM->GroupName);

	if(!GetGroups(InFSM->GroupName).Contains(InFSM))
	{
		GetGroups(InFSM->GroupName).Add(InFSM);
	}
}

void AFSMModule::UnregisterFSM(UFSMComponent* InFSM)
{
	if(!HasGroup(InFSM->GroupName)) return;

	if(GetGroups(InFSM->GroupName).Contains(InFSM))
	{
		GetGroups(InFSM->GroupName).Remove(InFSM);
	}
}

void AFSMModule::SwitchGroupStateByIndex(const FName InGroupName, int32 InStateIndex)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroups(InGroupName))
	{
		Iter->SwitchStateByIndex(InStateIndex);
	}
}

void AFSMModule::SwitchGroupStateByClass(const FName InGroupName, TSubclassOf<UFiniteStateBase> InStateClass)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroups(InGroupName))
	{
		Iter->SwitchStateByClass(InStateClass);
	}
}

void AFSMModule::SwitchGroupDefaultState(const FName InGroupName)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroups(InGroupName))
	{
		Iter->SwitchDefaultState();
	}
}

void AFSMModule::SwitchGroupFinalState(const FName InGroupName)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroups(InGroupName))
	{
		Iter->SwitchFinalState();
	}
}

void AFSMModule::SwitchGroupLastState(const FName InGroupName)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroups(InGroupName))
	{
		Iter->SwitchLastState();
	}
}

void AFSMModule::SwitchGroupNextState(const FName InGroupName)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroups(InGroupName))
	{
		Iter->SwitchNextState();
	}
}
