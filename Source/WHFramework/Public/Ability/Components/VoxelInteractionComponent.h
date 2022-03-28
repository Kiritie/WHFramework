// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "InteractionComponent.h"
#include "VoxelInteractionComponent.generated.h"

class IInteractionAgentInterface;
/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UVoxelInteractionComponent : public UInteractionComponent
{
	GENERATED_BODY()
		
public:
	UVoxelInteractionComponent(const FObjectInitializer& ObjectInitializer);

public:
	virtual bool DoInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;
};
