// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Interaction/InteractionAgentInterface.h"

#include "Ability/Components/InteractionComponent.h"

// Add default functionality here for any IInteractionInterface functions that are not pure virtual.

bool IInteractionAgentInterface::CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	return true;
}

void IInteractionAgentInterface::OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	GetInteractionComponent()->SetInteractionAgent(InInteractionAgent);
}
