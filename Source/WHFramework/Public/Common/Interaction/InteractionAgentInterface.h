// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

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

	friend class UInteractionComponent;

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual bool CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent) = 0;
	
	virtual bool DoInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent = nullptr);

protected:
	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) = 0;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) = 0;

	virtual void OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassivity) = 0;

protected:
	IInteractionAgentInterface* InteractingAgent;
	
public:
	template<class T>
	T* GetInteractingAgent() const
	{
		return Cast<T>(GetInteractingAgent());
	}

	virtual IInteractionAgentInterface* GetInteractingAgent() const { return InteractingAgent; }

	virtual void SetInteractingAgent(IInteractionAgentInterface* InInteractionAgent);

	virtual TArray<EInteractAction> GetInteractableActions(IInteractionAgentInterface* InInteractionAgent = nullptr);

	virtual UInteractionComponent* GetInteractionComponent() const = 0;
};
