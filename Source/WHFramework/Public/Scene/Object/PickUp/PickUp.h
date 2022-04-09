// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Asset/AssetModuleTypes.h"
#include "GameFramework/Actor.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Scene/Object/SceneObjectInterface.h"

#include "PickUp.generated.h"

class IPickerInterface;
class AVoxelChunk;
class UBoxComponent;
class UMeshComponent;
class URotatingMovementComponent;

/**
 * 可拾取项
 */
UCLASS()
class WHFRAMEWORK_API APickUp : public AActor, public ISceneObjectInterface, public ISaveDataInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APickUp();

protected:
	UPROPERTY(BlueprintReadWrite)
	FAbilityItem Item;

	ISceneContainerInterface* Container;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* BoxComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	URotatingMovementComponent* RotatingComponent;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	virtual void OnPickUp(IPickerInterface* InPicker);

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void LoadData(FSaveData* InSaveData) override;

	virtual FSaveData* ToData(bool bSaved = true) override;

	virtual void Initialize(FAbilityItem InItem, bool bPreview = false);

	virtual void RemoveFromContainer() override;

public:
	FAbilityItem& GetItem() { return Item; }
	
	virtual ISceneContainerInterface* GetContainer() const override { return Container; }

	virtual void SetContainer(ISceneContainerInterface* InContainer) override { Container = InContainer; }
	
	UBoxComponent* GetBoxComponent() const { return BoxComponent; }

	UMeshComponent* GetMeshComponent() const { return MeshComponent; }

	URotatingMovementComponent* GetRotatingComponent() const { return RotatingComponent; }
};
