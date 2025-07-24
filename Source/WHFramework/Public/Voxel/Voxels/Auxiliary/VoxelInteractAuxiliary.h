// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelAuxiliary.h"
#include "Common/Interaction/InteractionAgentInterface.h"
#include "VoxelInteractAuxiliary.generated.h"

class UBoxComponent;
class UVoxel;
class UInteractionComponent;

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

	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassive) override;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UBoxComponent* BoxComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UInteractionComponent* Interaction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bInteracting;
	
public:
	template<class T>
	T* GetInteractingAgent() const
	{
		return Cast<T>(GetInteractingAgent());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual AActor* GetInteractingAgent(TSubclassOf<AActor> InClass) const { return Cast<AActor>(GetInteractingAgent()); }

	virtual IInteractionAgentInterface* GetInteractingAgent() const override { return IInteractionAgentInterface::GetInteractingAgent(); }

	virtual EInteractAgentType GetInteractAgentType() const override { return EInteractAgentType::Passivity; }
	
	virtual UInteractionComponent* GetInteractionComponent() const override;
};
