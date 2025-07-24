// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Common/Base/WHActor.h"
#include "Common/Interaction/InteractionAgentInterface.h"

#include "AbilityPickUpBase.generated.h"

class UBoxComponent;
class UFollowingMovementComponent;
class IAbilityPickerInterface;
class AVoxelChunk;
class UMeshComponent;
class URotatingMovementComponent;
class UFallingMovementComponent;

/**
 * 可拾取项
 */
UCLASS()
class WHFRAMEWORK_API AAbilityPickUpBase : public AWHActor, public IInteractionAgentInterface
{
	GENERATED_BODY()
	
public:	
	AAbilityPickUpBase();
	
	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void OnInitialize_Implementation() override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

protected:
	virtual void OnPickUp(IAbilityPickerInterface* InPicker);

	virtual bool CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassive) override;

protected:
	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	virtual FBox GetComponentsBoundingBox(bool bNonColliding, bool bIncludeFromChildActors) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FAbilityItem Item;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UBoxComponent* BoxComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UInteractionComponent* Interaction;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	URotatingMovementComponent* RotatingMovement;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UFallingMovementComponent* FallingMovement;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UFollowingMovementComponent* FollowingMovement;

public:
	FAbilityItem& GetItem() { return Item; }
	
	virtual TScriptInterface<ISceneContainerInterface> GetContainer_Implementation() const override { return Container; }

	virtual void SetContainer_Implementation(const TScriptInterface<ISceneContainerInterface>& InContainer) override { Container = InContainer; }

	virtual UMeshComponent* GetMeshComponent() const { return nullptr; }

	virtual UBoxComponent* GetBoxComponent() const { return BoxComponent; }

	virtual URotatingMovementComponent* GetRotatingComponent() const { return RotatingMovement; }

	virtual IInteractionAgentInterface* GetInteractingAgent() const override { return IInteractionAgentInterface::GetInteractingAgent(); }

	template<class T>
	T* GetInteractingAgent() const
	{
		return Cast<T>(GetInteractingAgent());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual AActor* GetInteractingAgent(TSubclassOf<AActor> InClass) const { return GetDeterminesOutputObject(Cast<AActor>(GetInteractingAgent()), InClass); }

	virtual EInteractAgentType GetInteractAgentType() const override { return EInteractAgentType::Passivity; }

	virtual UInteractionComponent* GetInteractionComponent() const override;
};
