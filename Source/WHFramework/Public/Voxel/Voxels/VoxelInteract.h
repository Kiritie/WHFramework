// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Voxel.h"
#include "VoxelInteract.generated.h"

/**
 * ����ֲ��
 */
UCLASS()
class WHFRAMEWORK_API UVoxelInteract : public UVoxel
{
	GENERATED_BODY()

public:
	UVoxelInteract();
	
	//////////////////////////////////////////////////////////////////////////
	// Voxel
public:
	virtual void OnReset_Implementation() override;
	
	virtual void LoadData(const FString& InData) override;

	virtual FString ToData() override;

public:
	virtual void OnGenerate(IVoxelAgentInterface* InAgent) override;

	virtual void OnDestroy(IVoxelAgentInterface* InAgent) override;

	virtual void OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual void OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual void OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual void OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual bool OnAgentInteract(IVoxelAgentInterface* InAgent, EInputInteractAction InInteractAction, EInputInteractEvent InInteractEvent, const FVoxelHitResult& InHitResult) override;

	//////////////////////////////////////////////////////////////////////////
	// Interact
public:
	virtual bool Interact(IVoxelAgentInterface* InAgent);

	virtual void UnInteract(IVoxelAgentInterface* InAgent);
};
