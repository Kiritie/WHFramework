// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Components/CharacterInteractionComponent.h"

UCharacterInteractionComponent::UCharacterInteractionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

bool UCharacterInteractionComponent::DoInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	if(!Super::DoInteract(InInteractionAgent, InInteractAction)) return false;

	switch (InInteractAction)
	{
		case EInteractAction::Dialogue:
		{
			return true;
		}
		default: break;
	}
	return false;
}
