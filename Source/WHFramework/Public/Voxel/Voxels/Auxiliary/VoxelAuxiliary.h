// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Interaction/InteractionAgentInterface.h"
#include "GameFramework/Actor.h"
#include "SaveGame/Base/SaveDataInterface.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Voxel/VoxelModuleTypes.h"

#include "VoxelAuxiliary.generated.h"

class AVoxelChunk;
class UVoxel;

/**
 * ���ظ�����
 */
UCLASS()
class WHFRAMEWORK_API AVoxelAuxiliary : public AWHActor, public ISaveDataInterface, public IInteractionAgentInterface
{
	GENERATED_BODY()
	
public:	
	AVoxelAuxiliary();

public:
	virtual int32 GetLimit_Implementation() const override { return 10000; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual FSaveData* ToData(bool bRefresh) override;

public:
	virtual void OnEnterInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual void OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent) override;

	virtual bool CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;

	virtual void OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UVoxelInteractionComponent* Interaction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVoxelItem VoxelItem;

public:
	FVoxelItem& GetVoxelItem() { return VoxelItem; }

	virtual UInteractionComponent* GetInteractionComponent() const override;
};
