// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/CommonTypes.h"
#include "Components/BoxComponent.h"
#include "InteractionComponent.generated.h"

class IInteractionAgentInterface;
/**
 * 
 */
UCLASS(ClassGroup="Collision", editinlinenew, meta=(DisplayName="Interaction", BlueprintSpawnableComponent))
class WHFRAMEWORK_API UInteractionComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent(const FObjectInitializer& ObjectInitializer);

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
	virtual void AddInteractAction(EInteractAction InInteractAction);
		
	virtual void RemoveInteractAction(EInteractAction InInteractAction);
		
	virtual void ClearInteractActions();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<EInteractAction> InteractActions;

public:
	virtual bool IsInteractable() const;
	
	virtual void SetInteractable(bool bValue);

	virtual IInteractionAgentInterface* GetInteractionAgent() const;

	virtual TArray<EInteractAction> GetInteractActions() const { return InteractActions; }
};
