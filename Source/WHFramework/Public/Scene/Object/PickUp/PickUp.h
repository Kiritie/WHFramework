// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "DreamWorld/DreamWorld.h"
#include "GameFramework/Actor.h"
#include "PickUp.generated.h"

class AAbilityCharacterBase;
class AVoxelChunk;
class UBoxComponent;
class UMeshComponent;
class URotatingMovementComponent;

/**
 * 可拾取项
 */
UCLASS()
class DREAMWORLD_API APickUp : public AActor, public ISceneObject
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APickUp();

public:
	UPROPERTY(BlueprintReadWrite)
	FItem Item;

	UPROPERTY(BlueprintReadWrite)
	AVoxelChunk* OwnerChunk;

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

	virtual void OnPickUp(AAbilityCharacterBase* InPicker);

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void LoadData(FPickUpSaveData InPickUpData);

	FPickUpSaveData ToData(bool bSaved = true) const;

	virtual void Initialize(FItem InItem, bool bPreview = false);

	UBoxComponent* GetBoxComponent() const { return BoxComponent; }

	UMeshComponent* GetMeshComponent() const { return MeshComponent; }

	URotatingMovementComponent* GetRotatingComponent() const { return RotatingComponent; }
};
