// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


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
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	virtual void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) override;

	virtual void OnAgentEnter(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnAgentLeave(IInteractionAgentInterface* InInteractionAgent) override;
};
