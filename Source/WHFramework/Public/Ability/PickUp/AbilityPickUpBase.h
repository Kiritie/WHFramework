// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Asset/AssetModuleTypes.h"
#include "GameFramework/Actor.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "ObjectPool/ObjectPoolInterface.h"

#include "AbilityPickUpBase.generated.h"

class IAbilityPickerInterface;
class AVoxelChunk;
class UBoxComponent;
class UMeshComponent;
class URotatingMovementComponent;

/**
 * 可拾取项
 */
UCLASS()
class WHFRAMEWORK_API AAbilityPickUpBase : public AWHActor, public IObjectPoolInterface, public ISceneActorInterface, public ISaveDataInterface
{
	GENERATED_BODY()
	
public:	
	AAbilityPickUpBase();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Default")
	FAbilityItem Item;

	UPROPERTY(VisibleAnywhere, Category = "Default")
	TScriptInterface<ISceneContainerInterface> Container;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* BoxComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	URotatingMovementComponent* RotatingComponent;

public:
	virtual void Initialize(FAbilityItem InItem);

protected:
	UFUNCTION()
	virtual void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	virtual void OnPickUp(IAbilityPickerInterface* InPicker);

public:
	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams);

	virtual void OnDespawn_Implementation();

	virtual void LoadData(FSaveData* InSaveData) override;

	virtual FSaveData* ToData() override;

public:
	FAbilityItem& GetItem() { return Item; }
	
	virtual TScriptInterface<ISceneContainerInterface> GetContainer_Implementation() const override { return Container; }

	virtual void SetContainer_Implementation(const TScriptInterface<ISceneContainerInterface>& InContainer) override { Container = InContainer; }
	
	UBoxComponent* GetBoxComponent() const { return BoxComponent; }

	virtual UMeshComponent* GetMeshComponent() const { return nullptr; }

	URotatingMovementComponent* GetRotatingComponent() const { return RotatingComponent; }
};
