// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Voxel.h"
#include "VoxelWater.generated.h"

class AVoxelChunk;

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
	// Defaults
public:
	virtual void LoadData(const FString& InValue) override;

	virtual FString ToData() override;

	//////////////////////////////////////////////////////////////////////////
	// Events
public:
	virtual void OnTargetHit(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult) override;

	virtual void OnTargetEnter(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult) override;

	virtual void OnTargetStay(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult) override;

	virtual void OnTargetExit(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult) override;

	virtual bool OnMouseDown(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult) override;

	virtual bool OnMouseUp(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult) override;

	virtual bool OnMouseHold(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult) override;

	virtual void OnMouseHover(const FVoxelHitResult& InHitResult) override;
};
