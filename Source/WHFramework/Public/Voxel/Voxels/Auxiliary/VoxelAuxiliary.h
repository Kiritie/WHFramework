// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Interaction/InteractionAgentInterface.h"
#include "GameFramework/Actor.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelAuxiliary.generated.h"

class AVoxelChunk;
class UVoxel;

/**
 * ���ظ�����
 */
UCLASS()
class WHFRAMEWORK_API AVoxelAuxiliary : public AWHActor, public IInteractionAgentInterface, public ISceneActorInterface
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
	AVoxelChunk* Container;

public:
	FIndex GetVoxelIndex() const { return VoxelIndex; }

	FVoxelItem& GetVoxelItem() const;

	virtual TScriptInterface<ISceneContainerInterface> GetContainer_Implementation() const override;

	virtual void SetContainer_Implementation(const TScriptInterface<ISceneContainerInterface>& InContainer) override;

	virtual UInteractionComponent* GetInteractionComponent() const override;
};
