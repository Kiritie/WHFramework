// Fill out your copyright notice in the Description page of Project Settings.


#include "FSM/Components/FSMComponent.h"

#include "Common/CommonStatics.h"
#include "Debug/DebugModuleTypes.h"
#include "FSM/FSMModuleStatics.h"
#include "FSM/Base/FSMAgentInterface.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

// ParamSets default values
UFSMComponent::UFSMComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	bAutoSwitchDefault = false;
	GroupName = NAME_None;
	DefaultState = nullptr;
	FinalState = nullptr;
	CurrentState = nullptr;
	TargetState = nullptr;

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

	if(UCommonStatics::IsPlaying())
	{
		OnTermination();
	}
}

void UFSMComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	OnRefresh(DeltaTime);
}

void UFSMComponent::OnInitialize()
{
	UFSMModuleStatics::RegisterFSM(this);
	
	for(auto& Iter : States)
	{
		if(Iter)
		{
			const FName StateName = Iter->GetDefaultObject<UFiniteStateBase>()->GetStateName();
			if(!StateMap.Contains(StateName))
			{
				if(UFiniteStateBase* FiniteState = UObjectPoolModuleStatics::SpawnObject<UFiniteStateBase>(nullptr, nullptr, false, Iter))
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

void UFSMComponent::OnRefresh(float DeltaSeconds)
{
	if(CurrentState)
	{
		CurrentState->OnRefresh(DeltaSeconds);
	}
}

void UFSMComponent::OnTermination()
{
	UFSMModuleStatics::UnregisterFSM(this);
	
	for(auto& Iter : StateMap)
	{
		if(Iter.Value)
		{
			Iter.Value->OnTermination();
			UObjectPoolModuleStatics::DespawnObject(Iter.Value);
		}
	}
	
	StateMap.Empty();

	CurrentState = nullptr;
}

bool UFSMComponent::SwitchState(UFiniteStateBase* InState, const TArray<FParameter>& InParams)
{
	if(!bInitialized || InState == CurrentState || (InState && !HasState(InState)) || CurrentState != TargetState) return false;
	
	UFiniteStateBase* LastState = CurrentState;

	if(!LastState || LastState->OnPreLeave(InState))
	{
		if(!InState || InState->OnPreEnter(LastState, InParams))
		{
			TargetState = InState;
			if(LastState)
			{
				LastState->OnLeave(InState);
			}
			CurrentState = TargetState;
			if(InState)
			{
				InState->OnEnter(LastState, InParams);
			}
		}
	}
	if(CurrentState != LastState)
	{
		GetAgent<IFSMAgentInterface>()->OnFiniteStateChanged(CurrentState, LastState);
		OnStateChanged.Broadcast(CurrentState, LastState);
		if(!CurrentState)
		{
			RefreshState();
		}
		return true;
	}
	return false;
}

bool UFSMComponent::SwitchStateByIndex(int32 InStateIndex, const TArray<FParameter>& InParams)
{
	if(HasStateByIndex(InStateIndex))
	{
		return SwitchState(GetStateByIndex<UFiniteStateBase>(InStateIndex), InParams);
	}
	else if(bInitialized)
	{
		if(bShowDebugMessage)
		{
			WHLog(FString::Printf(TEXT("%s=>切换状态失败，不存在指定索引的状态: %d"), *GetAgent()->GetActorNameOrLabel(), InStateIndex), EDC_FSM, EDV_Warning);
		}
	}
	return false;
}

bool UFSMComponent::SwitchStateByName(const FName InStateName, const TArray<FParameter>& InParams)
{
	if(HasStateByName(InStateName))
	{
		return SwitchState(GetStateByName<UFiniteStateBase>(InStateName), InParams);
	}
	else if(bInitialized)
	{
		if(bShowDebugMessage)
		{
			WHLog(FString::Printf(TEXT("%s=>切换状态失败，不存在指定名称的状态: %s"), *GetAgent()->GetActorNameOrLabel(), *InStateName.ToString()), EDC_FSM, EDV_Warning);
		}
	}
	return false;
}

bool UFSMComponent::SwitchStateByClass(TSubclassOf<UFiniteStateBase> InStateClass, const TArray<FParameter>& InParams)
{
	if(HasStateByClass<UFiniteStateBase>(InStateClass))
	{
		return SwitchState(GetStateByClass<UFiniteStateBase>(InStateClass), InParams);
	}
	else if(bInitialized)
	{
		if(bShowDebugMessage)
		{
			WHLog(FString::Printf(TEXT("%s=>切换状态失败，不存在指定类型的状态: %s"), *GetAgent()->GetActorNameOrLabel(), InStateClass ? *InStateClass->GetName() : TEXT("None")), EDC_FSM, EDV_Warning);
		}
	}
	return false;
}

bool UFSMComponent::SwitchDefaultState(const TArray<FParameter>& InParams)
{
	if(DefaultState)
	{
		return SwitchStateByClass(DefaultState, InParams);
	}
	return false;
}

bool UFSMComponent::SwitchFinalState(const TArray<FParameter>& InParams)
{
	if(FinalState)
	{
		return SwitchStateByClass(FinalState, InParams);
	}
	return false;
}

bool UFSMComponent::SwitchLastState(const TArray<FParameter>& InParams)
{
	if(CurrentState && CurrentState->GetStateIndex() > 0)
	{
		return SwitchStateByIndex(CurrentState->GetStateIndex() - 1, InParams);
	}
	return false;
}

bool UFSMComponent::SwitchNextState(const TArray<FParameter>& InParams)
{
	if(CurrentState && CurrentState->GetStateIndex() < StateMap.Num() - 1)
	{
		return SwitchStateByIndex(CurrentState->GetStateIndex() + 1, InParams);
	}
	return false;
}

void UFSMComponent::RefreshState()
{
	GetAgent<IFSMAgentInterface>()->OnFiniteStateRefresh(CurrentState);
}

bool UFSMComponent::TerminateState(UFiniteStateBase* InState)
{
	if(InState == CurrentState) CurrentState = nullptr;

	bool bSuccess = false;
	if(StateMap.Contains(InState->GetStateName()))
	{
		StateMap.Remove(InState->GetStateName());
		InState->OnTermination();
		UObjectPoolModuleStatics::DespawnObject(InState);
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
	for(auto& Iter : StateMap)
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
	if(HasStateByIndex(InStateIndex))
	{
		return GetDeterminesOutputObject(GetStates()[InStateIndex], InStateClass);
	}
	return nullptr;
}

bool UFSMComponent::HasStateByName(const FName InStateName) const
{
	if(InStateName.IsNone()) return false;

	return StateMap.Contains(InStateName);
}

UFiniteStateBase* UFSMComponent::GetStateByName(const FName InStateName, TSubclassOf<UFiniteStateBase> InStateClass) const
{
	if(InStateName.IsNone()) return nullptr;

	if(HasStateByName(InStateName))
	{
		return GetDeterminesOutputObject(StateMap[InStateName], InStateClass);
	}
	return nullptr;
}

bool UFSMComponent::HasStateByClass(TSubclassOf<UFiniteStateBase> InStateClass) const
{
	if(!InStateClass) return false;

	const FName StateName = InStateClass->GetDefaultObject<UFiniteStateBase>()->GetStateName();
	return StateMap.Contains(StateName);
}

UFiniteStateBase* UFSMComponent::GetStateByClass(TSubclassOf<UFiniteStateBase> InStateClass) const
{
	if(HasStateByClass(InStateClass))
	{
		const FName StateName = InStateClass->GetDefaultObject<UFiniteStateBase>()->GetStateName();
		return GetDeterminesOutputObject(StateMap[StateName], InStateClass);
	}
	return nullptr;
}

bool UFSMComponent::IsCurrentState(UFiniteStateBase* InState) const
{
	return InState == CurrentState;
}

bool UFSMComponent::IsCurrentStateIndex(int32 InStateIndex) const
{
	return CurrentState && CurrentState->GetStateIndex() == InStateIndex;
}
