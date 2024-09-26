// Fill out your copyright notice in the Description page of Project Settings.


#include "Common/Interaction/InteractionAgentInterface.h"

#include "Common/Interaction/InteractionComponent.h"


// Add default functionality here for any IInteractionInterface functions that are not pure virtual.

bool IInteractionAgentInterface::IsInteractable(IInteractionAgentInterface* InInteractionAgent)
{
	switch(GetInteractAgentType())
	{
		case EInteractAgentType::Static:
		{
			switch(InInteractionAgent->GetInteractAgentType())
			{
				case EInteractAgentType::Movable:
				{
					return true;
				}
				default: break;
			}
			break;
		}
		case EInteractAgentType::Movable:
		{
			switch(InInteractionAgent->GetInteractAgentType())
			{
				case EInteractAgentType::Static:
				case EInteractAgentType::Movable:
				{
					return true;
				}
				default: break;
			}
			break;
		}
		default: break;
	}
	return false;
}

bool IInteractionAgentInterface::IsOverlapping(IInteractionAgentInterface* InInteractionAgent)
{
	return OverlappingAgents.Contains(InInteractionAgent);
}

bool IInteractionAgentInterface::EnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
	if(!InInteractionAgent) return false;
	
	return GetInteractionComponent()->OnAgentEnter(InInteractionAgent);
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

bool IInteractionAgentInterface::LeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
	if(!InInteractionAgent) InInteractionAgent = InteractingAgent;

	if(!InInteractionAgent) return false;
	
	return GetInteractionComponent()->OnAgentLeave(InInteractionAgent);
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
