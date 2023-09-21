// Fill out your copyright notice in the Description page of Project Settings.


#include "Common/Interaction/InteractionAgentInterface.h"

#include "Common/Interaction/InteractionComponent.h"


// Add default functionality here for any IInteractionInterface functions that are not pure virtual.

bool IInteractionAgentInterface::DoInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent)
{
	if(!InInteractionAgent) InInteractionAgent = InteractingAgent;

	if(!InInteractionAgent) return false;
	
	if(InInteractionAgent->CanInteract(InInteractAction, this))
	{
		InInteractionAgent->OnInteract(InInteractAction, this, true);
		OnInteract(InInteractAction, InInteractionAgent, false);
		return true;
	}
	return false;
}

void IInteractionAgentInterface::SetInteractingAgent(IInteractionAgentInterface* InInteractionAgent)
{
	if(InteractingAgent == InInteractionAgent) return;

	if(InInteractionAgent)
	{
		const auto InteractionAgent = InteractingAgent;
		OnEnterInteract(InInteractionAgent);
		InteractingAgent = InInteractionAgent;
		if(InteractionAgent) OnLeaveInteract(InteractionAgent);
		InInteractionAgent->SetInteractingAgent(this);
	}
	else if(InteractingAgent)
	{
		OnLeaveInteract(InteractingAgent);
		const auto InteractionAgent = InteractingAgent;
		InteractingAgent = nullptr;
		InteractionAgent->SetInteractingAgent(nullptr);
	}
}

TArray<EInteractAction> IInteractionAgentInterface::GetInteractableActions(IInteractionAgentInterface* InInteractionAgent)
{
	if(!InInteractionAgent) InInteractionAgent = InteractingAgent;

	if(!InInteractionAgent) return TArray<EInteractAction>();
	
	TArray<EInteractAction> ReturnValues;
	for(auto Iter : InInteractionAgent->GetInteractionComponent()->GetInteractActions())
	{
		if(InInteractionAgent->CanInteract(Iter, this))
		{
			ReturnValues.Add(Iter);
		}
	}
	return ReturnValues;
}
