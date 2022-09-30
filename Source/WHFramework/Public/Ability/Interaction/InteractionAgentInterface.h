// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/AbilityModuleTypes.h"
#include "UObject/Interface.h"
#include "InteractionAgentInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UInteractionAgentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IInteractionAgentInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) = 0;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) = 0;

	virtual bool CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) = 0;

	virtual void OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) = 0;

public:
	virtual class UInteractionComponent* GetInteractionComponent() const = 0;
};
