// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Voxel.h"
#include "VoxelWater.generated.h"

/**
 * ����ˮ
 */
UCLASS()
class WHFRAMEWORK_API UVoxelWater : public UVoxel
{
	GENERATED_BODY()

public:
	UVoxelWater();
	
	//////////////////////////////////////////////////////////////////////////
	// Voxel
public:
	virtual void LoadData(const FString& InData) override;

	virtual FString ToData() override;

public:
	virtual void OnGenerate(IVoxelAgentInterface* InAgent) override;

	virtual void OnDestroy(IVoxelAgentInterface* InAgent) override;

	virtual void OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual void OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual void OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual void OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual bool OnAgentInteract(IVoxelAgentInterface* InAgent, EInputInteractAction InInteractAction, const FVoxelHitResult& InHitResult) override;
};
