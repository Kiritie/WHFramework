// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "InteractionComponent.h"
#include "VitalityInteractionComponent.generated.h"

class IInteractionAgentInterface;
/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UVitalityInteractionComponent : public UInteractionComponent
{
	GENERATED_BODY()
		
public:
	UVitalityInteractionComponent(const FObjectInitializer& ObjectInitializer);

public:
	virtual bool DoInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;
};
