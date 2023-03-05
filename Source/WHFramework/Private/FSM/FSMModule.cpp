// Fill out your copyright notice in the Description page of Project Settings.


#include "FSM/FSMModule.h"

#include "FSM/Components/FSMComponent.h"
		
IMPLEMENTATION_MODULE(AFSMModule)

// ParamSets default values
AFSMModule::AFSMModule()
{
	ModuleName = FName("FSMModule");

	FSMGroupMap = TMap<FName, FFSMGroupInfo>();
}

AFSMModule::~AFSMModule()
{
	TERMINATION_MODULE(AFSMModule)
}

#if WITH_EDITOR
void AFSMModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AFSMModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
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

void AFSMModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();
}

void AFSMModule::RegisterFSM(UFSMComponent* InFSM)
{
	if(!HasFSMGroup(InFSM->GroupName)) FSMGroupMap.Add(InFSM->GroupName);

	if(!GetFSMGroupArray(InFSM->GroupName).Contains(InFSM))
	{
		GetFSMGroupArray(InFSM->GroupName).Add(InFSM);
	}
}

void AFSMModule::UnregisterFSM(UFSMComponent* InFSM)
{
	if(!HasFSMGroup(InFSM->GroupName)) return;

	if(GetFSMGroupArray(InFSM->GroupName).Contains(InFSM))
	{
		GetFSMGroupArray(InFSM->GroupName).Remove(InFSM);
	}
}

void AFSMModule::SwitchFSMGroupStateByIndex(const FName InFSMGroup, int32 InStateIndex)
{
	if(!HasFSMGroup(InFSMGroup)) return;

	for(auto Iter : GetFSMGroupArray(InFSMGroup))
	{
		Iter->SwitchStateByIndex(InStateIndex);
	}
}

void AFSMModule::SwitchFSMGroupStateByClass(const FName InFSMGroup, TSubclassOf<UFiniteStateBase> InStateClass)
{
	if(!HasFSMGroup(InFSMGroup)) return;

	for(auto Iter : GetFSMGroupArray(InFSMGroup))
	{
		Iter->SwitchStateByClass(InStateClass);
	}
}

void AFSMModule::SwitchFSMGroupDefaultState(const FName InFSMGroup)
{
	if(!HasFSMGroup(InFSMGroup)) return;

	for(auto Iter : GetFSMGroupArray(InFSMGroup))
	{
		Iter->SwitchDefaultState();
	}
}

void AFSMModule::SwitchFSMGroupFinalState(const FName InFSMGroup)
{
	if(!HasFSMGroup(InFSMGroup)) return;

	for(auto Iter : GetFSMGroupArray(InFSMGroup))
	{
		Iter->SwitchFinalState();
	}
}

void AFSMModule::SwitchFSMGroupLastState(const FName InFSMGroup)
{
	if(!HasFSMGroup(InFSMGroup)) return;

	for(auto Iter : GetFSMGroupArray(InFSMGroup))
	{
		Iter->SwitchLastState();
	}
}

void AFSMModule::SwitchFSMGroupNextState(const FName InFSMGroup)
{
	if(!HasFSMGroup(InFSMGroup)) return;

	for(auto Iter : GetFSMGroupArray(InFSMGroup))
	{
		Iter->SwitchNextState();
	}
}
