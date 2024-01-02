// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Common/Base/WHActor.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Scene/Actor/SceneActorInterface.h"

#include "AbilityPickUpBase.generated.h"

class IAbilityPickerInterface;
class AVoxelChunk;
class UBoxComponent;
class UMeshComponent;
class URotatingMovementComponent;
class UFallingMovementComponent;

/**
 * 可拾取项
 */
UCLASS()
class WHFRAMEWORK_API AAbilityPickUpBase : public AWHActor, public ISaveDataInterface
{
	GENERATED_BODY()
	
public:	
	AAbilityPickUpBase();

public:
	virtual void OnInitialize_Implementation() override;

protected:
	virtual int32 GetLimit_Implementation() const override { return 1000; }
	
	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData() override;

protected:
	virtual void OnPickUp(IAbilityPickerInterface* InPicker);

protected:
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FAbilityItem Item;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UBoxComponent* BoxComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	URotatingMovementComponent* RotatingComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UFallingMovementComponent* FallingComponent;

public:
	FAbilityItem& GetItem() { return Item; }
	
	virtual TScriptInterface<ISceneContainerInterface> GetContainer_Implementation() const override { return Container; }

	virtual void SetContainer_Implementation(const TScriptInterface<ISceneContainerInterface>& InContainer) override { Container = InContainer; }
	
	UBoxComponent* GetBoxComponent() const { return BoxComponent; }

	virtual UMeshComponent* GetMeshComponent() const { return nullptr; }

	URotatingMovementComponent* GetRotatingComponent() const { return RotatingComponent; }
};
