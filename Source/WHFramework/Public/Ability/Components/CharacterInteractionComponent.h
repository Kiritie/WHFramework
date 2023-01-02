// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "InteractionComponent.h"
#include "CharacterInteractionComponent.generated.h"

class IInteractionAgentInterface;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UCharacterInteractionComponent : public UInteractionComponent
{
	GENERATED_BODY()
		
public:
	UCharacterInteractionComponent(const FObjectInitializer& ObjectInitializer);

public:
	virtual bool DoInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;
};
