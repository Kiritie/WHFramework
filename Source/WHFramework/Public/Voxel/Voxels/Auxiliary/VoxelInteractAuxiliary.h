// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelAuxiliary.h"
#include "Ability/Interaction/InteractionAgentInterface.h"
#include "VoxelInteractAuxiliary.generated.h"

class UVoxel;

/**
 */
UCLASS()
class WHFRAMEWORK_API AVoxelInteractAuxiliary : public AVoxelAuxiliary, public IInteractionAgentInterface
{
	GENERATED_BODY()
	
public:
	AVoxelInteractAuxiliary();

protected:
	virtual void OnDespawn_Implementation(bool bRecovery) override;
	
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;
	
public:
	virtual bool CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent) override;

	virtual bool DoInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassivity) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UVoxelInteractionComponent* Interaction;

public:
	virtual IInteractionAgentInterface* GetInteractingAgent() const override { return IInteractionAgentInterface::GetInteractingAgent(); }

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InAgentClass"))
	virtual AActor* GetInteractingAgent(TSubclassOf<AActor> InAgentClass) const { return Cast<AActor>(GetInteractingAgent()); }
	
	virtual UInteractionComponent* GetInteractionComponent() const override;
};
