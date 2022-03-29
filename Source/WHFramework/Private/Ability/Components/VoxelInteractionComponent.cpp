// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Components/VoxelInteractionComponent.h"

UVoxelInteractionComponent::UVoxelInteractionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

bool UVoxelInteractionComponent::DoInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
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
