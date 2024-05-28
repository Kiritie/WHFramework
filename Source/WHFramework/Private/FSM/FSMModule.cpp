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

	bModuleRequired = true;

	ModuleNetworkComponent = UFSMModuleNetworkComponent::StaticClass();

	GroupMap = TMap<FName, FFSMGroup>();
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

void UFSMModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
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

	FFSMGroup& Group = GetGroup(InFSM->GroupName);
	if(Group.FSMArray.Contains(InFSM))
	{
		Group.FSMArray.Add(InFSM);
	}
}

void UFSMModule::UnregisterFSM(UFSMComponent* InFSM)
{
	if(!HasGroup(InFSM->GroupName)) return;

	FFSMGroup& Group = GetGroup(InFSM->GroupName);
	if(Group.FSMArray.Contains(InFSM))
	{
		Group.FSMArray.Remove(InFSM);
	}
	if(Group.FSMArray.IsEmpty())
	{
		GroupMap.Remove(InFSM->GroupName);
	}
}

void UFSMModule::SwitchGroupStateByIndex(const FName InGroupName, int32 InStateIndex, const TArray<FParameter>& InParams)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroup(InGroupName).FSMArray)
	{
		Iter->SwitchStateByIndex(InStateIndex, InParams);
	}
}

void UFSMModule::SwitchGroupStateByName(const FName InGroupName, const FName InStateName, const TArray<FParameter>& InParams)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroup(InGroupName).FSMArray)
	{
		Iter->SwitchStateByName(InStateName, InParams);
	}
}

void UFSMModule::SwitchGroupStateByClass(const FName InGroupName, TSubclassOf<UFiniteStateBase> InStateClass, const TArray<FParameter>& InParams)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroup(InGroupName).FSMArray)
	{
		Iter->SwitchStateByClass(InStateClass, InParams);
	}
}

void UFSMModule::SwitchGroupDefaultState(const FName InGroupName, const TArray<FParameter>& InParams)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroup(InGroupName).FSMArray)
	{
		Iter->SwitchDefaultState(InParams);
	}
}

void UFSMModule::SwitchGroupFinalState(const FName InGroupName, const TArray<FParameter>& InParams)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroup(InGroupName).FSMArray)
	{
		Iter->SwitchFinalState(InParams);
	}
}

void UFSMModule::SwitchGroupLastState(const FName InGroupName, const TArray<FParameter>& InParams)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroup(InGroupName).FSMArray)
	{
		Iter->SwitchLastState(InParams);
	}
}

void UFSMModule::SwitchGroupNextState(const FName InGroupName, const TArray<FParameter>& InParams)
{
	if(!HasGroup(InGroupName)) return;

	for(auto Iter : GetGroup(InGroupName).FSMArray)
	{
		Iter->SwitchNextState(InParams);
	}
}
