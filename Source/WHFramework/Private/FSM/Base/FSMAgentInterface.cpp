// Fill out your copyright notice in the Description page of Project Settings.


#include "FSM/Base/FSMAgentInterface.h"

#include "FSM/Components/FSMComponent.h"

// Add default functionality here for any IModule functions that are not pure virtual.

bool IFSMAgentInterface::SwitchFiniteState(UFiniteStateBase* InState, const TArray<FParameter>& InParams)
{
	return GetFSMComponent()->SwitchState(InState, InParams);
}

bool IFSMAgentInterface::SwitchFiniteStateByIndex(int32 InStateIndex, const TArray<FParameter>& InParams)
{
	return GetFSMComponent()->SwitchStateByIndex(InStateIndex, InParams);
}

bool IFSMAgentInterface::SwitchFiniteStateByName(const FName InStateName, const TArray<FParameter>& InParams)
{
	return GetFSMComponent()->SwitchStateByName(InStateName, InParams);
}

bool IFSMAgentInterface::SwitchFiniteStateByClass(TSubclassOf<UFiniteStateBase> InStateClass, const TArray<FParameter>& InParams)
{
	return GetFSMComponent()->SwitchStateByClass(InStateClass, InParams);
}

bool IFSMAgentInterface::SwitchDefaultFiniteState(const TArray<FParameter>& InParams)
{
	return GetFSMComponent()->SwitchDefaultState(InParams);
}

bool IFSMAgentInterface::SwitchFinalFiniteState(const TArray<FParameter>& InParams)
{
	return GetFSMComponent()->SwitchFinalState(InParams);
}

void IFSMAgentInterface::RefreshFiniteState()
{
	GetFSMComponent()->RefreshState();
}

bool IFSMAgentInterface::TerminateFiniteState(UFiniteStateBase* InState)
{
	return GetFSMComponent()->TerminateState(InState);
}

bool IFSMAgentInterface::HasFiniteState(UFiniteStateBase* InState) const
{
	return GetFSMComponent()->HasState(InState);
}

bool IFSMAgentInterface::HasFiniteStateByIndex(int32 InStateIndex) const
{
	return GetFSMComponent()->HasStateByIndex(InStateIndex);
}

UFiniteStateBase* IFSMAgentInterface::GetFiniteStateByIndex(int32 InStateIndex, TSubclassOf<UFiniteStateBase> InStateClass) const
{
	return GetFSMComponent()->GetStateByIndex(InStateIndex, InStateClass);
}

bool IFSMAgentInterface::HasFiniteStateByName(const FName InStateName) const
{
	return GetFSMComponent()->HasStateByName(InStateName);
}

UFiniteStateBase* IFSMAgentInterface::GetFiniteStateByName(const FName InStateName, TSubclassOf<UFiniteStateBase> InStateClass) const
{
	return GetFSMComponent()->GetStateByName(InStateName, InStateClass);
}

bool IFSMAgentInterface::HasFiniteStateByClass(TSubclassOf<UFiniteStateBase> InStateClass) const
{
	return GetFSMComponent()->HasStateByClass(InStateClass);
}

UFiniteStateBase* IFSMAgentInterface::GetFiniteStateByClass(TSubclassOf<UFiniteStateBase> InStateClass) const
{
	return GetFSMComponent()->GetStateByClass(InStateClass);
}

bool IFSMAgentInterface::IsCurrentFiniteState(UFiniteStateBase* InState) const
{
	return GetFSMComponent()->IsCurrentState(InState);
}

bool IFSMAgentInterface::IsCurrentFiniteStateIndex(int32 InStateIndex) const
{
	return GetFSMComponent()->IsCurrentStateIndex(InStateIndex);
}

bool IFSMAgentInterface::IsCurrentFiniteStateClass(TSubclassOf<UFiniteStateBase> InStateClass) const
{
	return GetFSMComponent()->IsCurrentStateClass(InStateClass);
}

UFiniteStateBase* IFSMAgentInterface::GetCurrentFiniteState(TSubclassOf<UFiniteStateBase> InClass) const
{
	return GetFSMComponent()->GetCurrentState(InClass);
}
