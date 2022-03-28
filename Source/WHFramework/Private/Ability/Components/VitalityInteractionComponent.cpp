// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Components/VitalityInteractionComponent.h"

UVitalityInteractionComponent::UVitalityInteractionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

bool UVitalityInteractionComponent::DoInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
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
