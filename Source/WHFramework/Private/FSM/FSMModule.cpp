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
	if (!::IsValid(InFSM) || InFSM->GetWorld() != GetWorld()) return;

	UnregisterFSM(InFSM);
	FFSMGroup& Group = GroupMap.FindOrAdd(InFSM->GroupName);
	Group.FSMArray.AddUnique(InFSM);
}

void UFSMModule::UnregisterFSM(UFSMComponent* InFSM)
{
	for (auto Iterator = GroupMap.CreateIterator(); Iterator; ++Iterator)
	{
		Iterator.Value().FSMArray.RemoveAll([InFSM](const UFSMComponent* Component)
		{
			return Component == InFSM || !::IsValid(Component);
		});
		if (Iterator.Value().FSMArray.IsEmpty())
		{
			Iterator.RemoveCurrent();
		}
	}
}

void UFSMModule::SwitchGroupStateByIndex(const FName InGroupName, int32 InStateIndex, const TArray<FParameter>& InParams)
{
	if(!HasGroup(InGroupName)) return;

	for (UFSMComponent* Iter : GetGroup(InGroupName).FSMArray)
	{
		if (!::IsValid(Iter) || !Iter->CanExecuteStateLogic()) continue;
		Iter->SwitchStateByIndex(InStateIndex, InParams);
	}
}

void UFSMModule::SwitchGroupStateByName(const FName InGroupName, const FName InStateName, const TArray<FParameter>& InParams)
{
	if(!HasGroup(InGroupName)) return;

	for (UFSMComponent* Iter : GetGroup(InGroupName).FSMArray)
	{
		if (!::IsValid(Iter) || !Iter->CanExecuteStateLogic()) continue;
		Iter->SwitchStateByName(InStateName, InParams);
	}
}

void UFSMModule::SwitchGroupStateByClass(const FName InGroupName, TSubclassOf<UFiniteStateBase> InStateClass, const TArray<FParameter>& InParams)
{
	if(!HasGroup(InGroupName)) return;

	for (UFSMComponent* Iter : GetGroup(InGroupName).FSMArray)
	{
		if (!::IsValid(Iter) || !Iter->CanExecuteStateLogic()) continue;
		Iter->SwitchStateByClass(InStateClass, InParams);
	}
}

void UFSMModule::SwitchGroupDefaultState(const FName InGroupName, const TArray<FParameter>& InParams)
{
	if(!HasGroup(InGroupName)) return;

	for (UFSMComponent* Iter : GetGroup(InGroupName).FSMArray)
	{
		if (!::IsValid(Iter) || !Iter->CanExecuteStateLogic()) continue;
		Iter->SwitchDefaultState(InParams);
	}
}

void UFSMModule::SwitchGroupFinalState(const FName InGroupName, const TArray<FParameter>& InParams)
{
	if(!HasGroup(InGroupName)) return;

	for (UFSMComponent* Iter : GetGroup(InGroupName).FSMArray)
	{
		if (!::IsValid(Iter) || !Iter->CanExecuteStateLogic()) continue;
		Iter->SwitchFinalState(InParams);
	}
}

void UFSMModule::SwitchGroupLastState(const FName InGroupName, const TArray<FParameter>& InParams)
{
	if(!HasGroup(InGroupName)) return;

	for (UFSMComponent* Iter : GetGroup(InGroupName).FSMArray)
	{
		if (!::IsValid(Iter) || !Iter->CanExecuteStateLogic()) continue;
		Iter->SwitchLastState(InParams);
	}
}

void UFSMModule::SwitchGroupNextState(const FName InGroupName, const TArray<FParameter>& InParams)
{
	if(!HasGroup(InGroupName)) return;

	for (UFSMComponent* Iter : GetGroup(InGroupName).FSMArray)
	{
		if (!::IsValid(Iter) || !Iter->CanExecuteStateLogic()) continue;
		Iter->SwitchNextState(InParams);
	}
}
