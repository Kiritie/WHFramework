// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/AbilityModuleTypes.h"
#include "Components/BoxComponent.h"
#include "InteractionComponent.generated.h"

class IInteractionAgentInterface;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UInteractionComponent : public UBoxComponent
{
	GENERATED_BODY()
		
public:
	UInteractionComponent(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<EInteractAction> InteractActions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EInteractAction InteractingAction;
	
	IInteractionAgentInterface* OverlappingAgent;

	IInteractionAgentInterface* InteractionAgent;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	virtual void OnAgentEnter(IInteractionAgentInterface* InInteractionAgent);

	virtual void OnAgentLeave(IInteractionAgentInterface* InInteractionAgent);

public:
	virtual bool DoInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction);

public:
	void AddInteractionAction(EInteractAction InInteractAction);
		
	void RemoveInteractionAction(EInteractAction InInteractAction);
		
	TArray<EInteractAction> GetValidInteractActions(IInteractionAgentInterface* InInteractionAgent) const;
	
	EInteractAction GetInteractingAction() const { return InteractingAction; }

	IInteractionAgentInterface* GetOverlappingAgent() const { return OverlappingAgent; }

	IInteractionAgentInterface* GetInteractionOwner() const;

	IInteractionAgentInterface* GetInteractionAgent() const { return InteractionAgent; }

	void SetInteractionAgent(IInteractionAgentInterface* InInteractionAgent);
};
