// Fill out your copyright notice in the Description page of Project Settings.


#include "FSM/Components/FSMComponent.h"

#include "Debug/DebugModuleTypes.h"
#include "FSM/FSMModuleBPLibrary.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"

// ParamSets default values
UFSMComponent::UFSMComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	States = TArray<TSubclassOf<UFiniteStateBase>>();

	DefaultState = nullptr;
	FinalState = nullptr;
	CurrentState = nullptr;
	GroupName = NAME_None;

	StateMap = TMap<FName, UFiniteStateBase*>();
}

void UFSMComponent::BeginPlay()
{
	Super::BeginPlay();

}

void UFSMComponent::PostLoad()
{
	Super::PostLoad();

	OnInitialize();
}

void UFSMComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	OnTermination();
}

void UFSMComponent::OnInitialize()
{
	UFSMModuleBPLibrary::RegisterFSM(this);
	
	for(auto Iter : States)
	{
		if(Iter)
		{
			const FName StateName = Iter->GetDefaultObject<UFiniteStateBase>()->GetStateName();
			if(!StateMap.Contains(StateName))
			{
				if(UFiniteStateBase* FiniteState = UObjectPoolModuleBPLibrary::SpawnObject<UFiniteStateBase>(nullptr, Iter))
				{
					FiniteState->OnInitialize(this, StateMap.Num());
					StateMap.Add(StateName, FiniteState);
				}
			}
		}
	}
	SwitchDefaultState();
}

void UFSMComponent::OnRefresh()
{
	if(CurrentState)
	{
		CurrentState->OnRefresh();
	}
}

void UFSMComponent::OnTermination()
{
	UFSMModuleBPLibrary::UnregisterFSM(this);
	
	for(auto Iter : StateMap)
	{
		if(Iter.Value)
		{
			Iter.Value->OnTermination();
			UObjectPoolModuleBPLibrary::DespawnObject(Iter.Value);
		}
	}
	
	StateMap.Empty();

	CurrentState = nullptr;
}

void UFSMComponent::SwitchState(UFiniteStateBase* InState)
{
	if(InState == CurrentState) return;
	
	UFiniteStateBase* LastState = CurrentState;

	CurrentState = InState;

	if(!InState || GetStates().Contains(InState) && InState->OnValidate())
	{
		if(LastState)
		{
			LastState->OnLeave(InState);
		}
		if(InState)
		{
			InState->OnEnter(LastState);
		}
	}
	GetAgent<IFSMAgentInterface>()->OnFiniteStateChanged(CurrentState);
	OnStateChanged.Broadcast(CurrentState);
}

void UFSMComponent::SwitchStateByIndex(int32 InStateIndex)
{
	if(HasStateByIndex(InStateIndex))
	{
		SwitchState(GetStateByIndex<UFiniteStateBase>(InStateIndex));
	}
	else
	{
		WHLog(WH_FSM, Warning, TEXT("%s=>切换状态失败，不存在指定索引的状态: %d"), *GetAgent()->GetActorLabel(), InStateIndex);
	}
}

void UFSMComponent::SwitchStateByClass(TSubclassOf<UFiniteStateBase> InStateClass)
{
	if(HasStateByClass<UFiniteStateBase>(InStateClass))
	{
		SwitchState(GetStateByClass<UFiniteStateBase>(InStateClass));
	}
	else
	{
		WHLog(WH_FSM, Warning, TEXT("%s=>切换状态失败，不存在指定类型的状态: %s"), *GetAgent()->GetActorLabel(), InStateClass ? *InStateClass->GetName() : TEXT("None"));
	}
}

void UFSMComponent::SwitchDefaultState()
{
	if(DefaultState)
	{
		SwitchStateByClass(DefaultState);
	}
}

void UFSMComponent::SwitchFinalState()
{
	if(FinalState)
	{
		SwitchStateByClass(FinalState);
	}
}

void UFSMComponent::SwitchLastState()
{
	if(CurrentState && CurrentState->GetStateIndex() > 0)
	{
		SwitchStateByIndex(CurrentState->GetStateIndex() - 1);
	}
}

void UFSMComponent::SwitchNextState()
{
	if(CurrentState && CurrentState->GetStateIndex() < StateMap.Num() - 1)
	{
		SwitchStateByIndex(CurrentState->GetStateIndex() + 1);
	}
}

void UFSMComponent::TerminateState(UFiniteStateBase* InState)
{
	if(InState == CurrentState) CurrentState = nullptr;
	
	if(StateMap.Contains(InState->GetStateName()))
	{
		StateMap.Remove(InState->GetStateName());
		InState->OnTermination();
		UObjectPoolModuleBPLibrary::DespawnObject(InState);
	}
	const auto TempStates = GetStates();
	for(int32 i = 0; i < TempStates.Num(); i++)
	{
		TempStates[i]->SetStateIndex(i);
	}
}

bool UFSMComponent::HasStateByIndex(int32 InStateIndex) const
{
	return GetStates().IsValidIndex(InStateIndex);
}

UFiniteStateBase* UFSMComponent::GetStateByIndex(int32 InStateIndex, TSubclassOf<UFiniteStateBase> InStateClass) const
{
	return GetStateByIndex<UFiniteStateBase>(InStateIndex);
}

bool UFSMComponent::HasStateByClass(TSubclassOf<UFiniteStateBase> InStateClass) const
{
	return HasStateByClass<UFiniteStateBase>(InStateClass);
}

UFiniteStateBase* UFSMComponent::GetStateByClass(TSubclassOf<UFiniteStateBase> InStateClass) const
{
	return GetStateByClass<UFiniteStateBase>(InStateClass);
}

bool UFSMComponent::IsCurrentState(UFiniteStateBase* InState) const
{
	return InState == CurrentState;
}

bool UFSMComponent::IsCurrentStateIndex(int32 InStateIndex) const
{
	return CurrentState && CurrentState->GetStateIndex() == InStateIndex;
}
