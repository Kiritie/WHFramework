// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "DreamWorld/DreamWorld.h"
#include "VoxelAuxiliary/VoxelVirtualAuxiliary.h"
#include "VoxelDoorAuxiliary.generated.h"

class UVoxel;

/**
 * �Ÿ�����
 */
UCLASS()
class WHFRAMEWORK_API AVoxelDoorAuxiliary : public AVoxelVirtualAuxiliary
{
	GENERATED_BODY()
	
public:
	// Sets default values for this actor's properties
	AVoxelDoorAuxiliary();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	virtual void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) override;

public:	
	virtual void Initialize(AVoxelChunk* InOwnerChunk, FIndex InVoxelIndex) override;

	virtual bool CanInteract(IInteraction* InInteractionTarget, EInteractAction InInteractAction) override;

	virtual void OnInteract(IInteraction* InTrigger, EInteractAction InInteractAction) override;

	virtual void OpenDoor();

	virtual void CloseDoor();
};
