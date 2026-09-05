// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/CommonModuleTypes.h"
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
	virtual bool IsInteractable(IInteractionAgentInterface* InInteractionAgent);

	virtual bool IsOverlapping(IInteractionAgentInterface* InInteractionAgent);

	virtual bool EnterInteract(IInteractionAgentInterface* InInteractionAgent);

	virtual bool LeaveInteract(IInteractionAgentInterface* InInteractionAgent = nullptr);

protected:
	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) = 0;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) = 0;

protected:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnBeginHover();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnHovering();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnEndHover();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnSelected();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnDeselected();

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void IsHovering();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void IsSelected();

protected:
	TArray<IInteractionAgentInterface*> OverlappingAgents;
	
	IInteractionAgentInterface* InteractingAgent = nullptr;
	
public:
	virtual EInteractAgentType GetInteractAgentType() const { return EInteractAgentType::None; }

	virtual TArray<IInteractionAgentInterface*>& GetOverlappingAgents() { return OverlappingAgents; }

	virtual IInteractionAgentInterface* GetInteractingAgent() const { return InteractingAgent; }

	virtual bool SetInteractingAgent(IInteractionAgentInterface* InInteractionAgent, bool bForce = false);

	virtual UInteractionComponent* GetInteractionComponent() const = 0;
};
