// Fill out your copyright notice in the Description page of Project Settings.


#include "FSM/Components/FSMComponent.h"

#include "Debug/DebugModuleTypes.h"

// ParamSets default values
UFSMComponent::UFSMComponent()
{
	States = TArray<TSubclassOf<UFiniteStateBase>>();

	DefaultState = nullptr;
	FinalState = nullptr;
	CurrentState = nullptr;

	StateMap = TMap<FName, UFiniteStateBase*>();
}

void UFSMComponent::BeginPlay()
{
	Super::BeginPlay();

	for(auto Iter : States)
	{
		if(Iter)
		{
			const FName StateName = Iter->GetDefaultObject<UFiniteStateBase>()->StateName;
			if(!StateMap.Contains(StateName))
			{
				if(UFiniteStateBase* FiniteState = NewObject<UFiniteStateBase>(this, Iter))
				{
					FiniteState->OnInitialize(this);
					StateMap.Add(StateName, FiniteState);
				}
			}
		}
	}

	SwitchDefaultState();
}

void UFSMComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(CurrentState)
	{
		CurrentState->OnRefresh();
	}
}

void UFSMComponent::SwitchState(UFiniteStateBase* InState)
{
	if(InState && InState != CurrentState)
	{
		if(CurrentState)
		{
			CurrentState->OnLeave(InState);
		}
		InState->OnEnter(CurrentState);

		CurrentState = InState;
	}
}

void UFSMComponent::SwitchStateByIndex(int32 InStateIndex)
{
	if(HasStateByIndex(InStateIndex))
	{
		SwitchState(GetStateByIndex<UFiniteStateBase>(InStateIndex));
	}
	else
	{
		WHLog(WH_FSM, Warning, TEXT("切换状态失败，不存在指定索引的状态: %d"), InStateIndex);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, FString::Printf(TEXT("切换状态失败，不存在指定索引的状态: %d"), InStateIndex));
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
		WHLog(WH_FSM, Warning, TEXT("切换状态失败，不存在指定类型的状态: %s"), InStateClass ? *InStateClass->GetName() : TEXT("None"));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, FString::Printf(TEXT("切换状态失败，不存在指定类型的状态: %s"), InStateClass ? *InStateClass->GetName() : TEXT("None")));
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
	if(CurrentState && CurrentState->StateIndex > 0)
	{
		SwitchStateByIndex(CurrentState->StateIndex - 1);
	}
}

void UFSMComponent::SwitchNextState()
{
	if(CurrentState && CurrentState->StateIndex < States.Num() - 1)
	{
		SwitchStateByIndex(CurrentState->StateIndex + 1);
	}
}

void UFSMComponent::ClearAllState()
{
	for(auto Iter : States)
	{
		if(Iter)
		{
			Iter->ConditionalBeginDestroy();
		}
	}
	
	States.Empty();

	CurrentState = nullptr;

	Modify();
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

bool UFSMComponent::IsCurrentStateIndex(int32 InStateIndex) const
{
	return CurrentState && CurrentState->StateIndex == InStateIndex;
}
