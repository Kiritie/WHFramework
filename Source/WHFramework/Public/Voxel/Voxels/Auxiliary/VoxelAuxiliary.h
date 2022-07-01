// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Interaction/InteractionAgentInterface.h"
#include "GameFramework/Actor.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelAuxiliary.generated.h"

class AVoxelChunk;
class UVoxel;

/**
 * ���ظ�����
 */
UCLASS()
class WHFRAMEWORK_API AVoxelAuxiliary : public AWHActor, public IInteractionAgentInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AVoxelAuxiliary();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	virtual void Initialize(AVoxelChunk* InOwnerChunk, FIndex InVoxelIndex);

	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual bool CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;

	virtual void OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UVoxelInteractionComponent* Interaction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FIndex VoxelIndex;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	AVoxelChunk* OwnerChunk;

public:
	FIndex GetVoxelIndex() const { return VoxelIndex; }

	FVoxelItem& GetVoxelItem() const;

	virtual UInteractionComponent* GetInteractionComponent() const override;
};
