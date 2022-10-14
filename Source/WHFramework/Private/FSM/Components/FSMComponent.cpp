// Fill out your copyright notice in the Description page of Project Settings.


#include "FSM/Components/FSMComponent.h"

#include "Debug/DebugModuleTypes.h"
#include "FSM/FSMModuleBPLibrary.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"

// ParamSets default values
UFSMComponent::UFSMComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	bAutoSwitchDefault = false;
	GroupName = NAME_None;
	DefaultState = nullptr;
	FinalState = nullptr;
	CurrentState = nullptr;

	States = TArray<TSubclassOf<UFiniteStateBase>>();

	StateMap = TMap<FName, UFiniteStateBase*>();

	bShowDebugMessage = false;

	bInitialized = false;
}

void UFSMComponent::BeginPlay()
{
	Super::BeginPlay();

	OnInitialize();
}

void UFSMComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	OnTermination();
}

void UFSMComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	OnRefresh();
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

	bInitialized = true;

	if(bAutoSwitchDefault)
	{
		SwitchDefaultState();
	}
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

bool UFSMComponent::SwitchState(UFiniteStateBase* InState)
{
	if(!bInitialized || InState && !HasState(InState)) return false;
	
	if(InState == CurrentState) return true;

	UFiniteStateBase* LastState = CurrentState;

	if(!LastState || LastState->OnLeaveValidate(InState))
	{
		if(!InState || InState->OnEnterValidate(LastState))
		{
			if(LastState)
			{
				LastState->OnLeave(InState);
				CurrentState = nullptr;
			}
			if(InState)
			{
				CurrentState = InState;
				InState->OnEnter(LastState);
			}
			else
			{
				CurrentState = nullptr;
			}
		}
	}
	if(CurrentState != LastState)
	{
		GetAgent<IFSMAgentInterface>()->OnFiniteStateChanged(CurrentState);
		OnStateChanged.Broadcast(CurrentState);
		return true;
	}
	return false;
}

bool UFSMComponent::SwitchStateByIndex(int32 InStateIndex)
{
	if(HasStateByIndex(InStateIndex))
	{
		return SwitchState(GetStateByIndex<UFiniteStateBase>(InStateIndex));
	}
	else if(bInitialized)
	{
		if(bShowDebugMessage)
		{
			WHLog(WH_FSM, Warning, TEXT("%s=>切换状态失败，不存在指定索引的状态: %d"), *GetAgent()->GetActorLabel(), InStateIndex);
		}
	}
	return false;
}

bool UFSMComponent::SwitchStateByClass(TSubclassOf<UFiniteStateBase> InStateClass)
{
	if(HasStateByClass<UFiniteStateBase>(InStateClass))
	{
		return SwitchState(GetStateByClass<UFiniteStateBase>(InStateClass));
	}
	else if(bInitialized)
	{
		if(bShowDebugMessage)
		{
			WHLog(WH_FSM, Warning, TEXT("%s=>切换状态失败，不存在指定类型的状态: %s"), *GetAgent()->GetActorLabel(), InStateClass ? *InStateClass->GetName() : TEXT("None"));
		}
	}
	return false;
}

bool UFSMComponent::SwitchDefaultState()
{
	if(DefaultState)
	{
		return SwitchStateByClass(DefaultState);
	}
	return false;
}

bool UFSMComponent::SwitchFinalState()
{
	if(FinalState)
	{
		return SwitchStateByClass(FinalState);
	}
	return false;
}

bool UFSMComponent::SwitchLastState()
{
	if(CurrentState && CurrentState->GetStateIndex() > 0)
	{
		return SwitchStateByIndex(CurrentState->GetStateIndex() - 1);
	}
	return false;
}

bool UFSMComponent::SwitchNextState()
{
	if(CurrentState && CurrentState->GetStateIndex() < StateMap.Num() - 1)
	{
		return SwitchStateByIndex(CurrentState->GetStateIndex() + 1);
	}
	return false;
}

bool UFSMComponent::TerminateState(UFiniteStateBase* InState)
{
	if(InState == CurrentState) CurrentState = nullptr;

	bool bSuccess = false;
	if(StateMap.Contains(InState->GetStateName()))
	{
		StateMap.Remove(InState->GetStateName());
		InState->OnTermination();
		UObjectPoolModuleBPLibrary::DespawnObject(InState);
		bSuccess = true;
	}
	const auto TempStates = GetStates();
	for(int32 i = 0; i < TempStates.Num(); i++)
	{
		TempStates[i]->SetStateIndex(i);
	}
	return bSuccess;
}

bool UFSMComponent::HasState(UFiniteStateBase* InState) const
{
	for(auto Iter : StateMap)
	{
		if(Iter.Value == InState)
		{
			return true;
		}
	}
	return false;
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
