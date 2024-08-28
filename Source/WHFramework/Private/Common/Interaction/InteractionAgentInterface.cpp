// Fill out your copyright notice in the Description page of Project Settings.


#include "Common/Interaction/InteractionAgentInterface.h"

#include "Common/Interaction/InteractionComponent.h"


// Add default functionality here for any IInteractionInterface functions that are not pure virtual.

bool IInteractionAgentInterface::IsOverlapping(IInteractionAgentInterface* InInteractionAgent)
{
	return OverlappingAgents.Contains(InInteractionAgent);
}

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

bool IInteractionAgentInterface::SetInteractingAgent(IInteractionAgentInterface* InInteractionAgent, bool bForce)
{
	if(InteractingAgent == InInteractionAgent) return false;

	if(InInteractionAgent)
	{
		if((!InteractingAgent && !InInteractionAgent->InteractingAgent) || bForce)
		{
			const auto LastInteractingAgent = InteractingAgent;
			InteractingAgent = InInteractionAgent;
			if(LastInteractingAgent) OnLeaveInteract(LastInteractingAgent);
			OnEnterInteract(InteractingAgent);
			InteractingAgent->SetInteractingAgent(this, true);
			return true;
		}
	}
	else
	{
		if((InteractingAgent && InteractingAgent->InteractingAgent == this) || bForce)
		{
			const auto LastInteractingAgent = InteractingAgent;
			InteractingAgent = nullptr;
			if(LastInteractingAgent) OnLeaveInteract(LastInteractingAgent);
			if(LastInteractingAgent) LastInteractingAgent->SetInteractingAgent(nullptr, true);
			return true;
		}
	}
	return false;
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
