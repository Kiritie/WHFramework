// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "DreamWorld/DreamWorld.h"
#include "VoxelAuxiliary/VoxelAuxiliary.h"
#include "VoxelVirtualAuxiliary.generated.h"

class UVoxel;
class UBoxComponent;

/**
 * �������ظ�����
 */
UCLASS()
class WHFRAMEWORK_API AVoxelVirtualAuxiliary : public AVoxelAuxiliary
{
	GENERATED_BODY()
	
public:
	// Sets default values for this actor's properties
	AVoxelVirtualAuxiliary();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* BoxComponent;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
	virtual void Initialize(AVoxelChunk* InOwnerChunk, FIndex InVoxelIndex) override;
};
