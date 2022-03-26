// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "DreamWorld/DreamWorld.h"
#include "Voxel/Voxel.h"
#include "VoxelTorch.generated.h"

class AVoxelChunk;

/**
 * ���ػ��
 */
UCLASS()
class WHFRAMEWORK_API UVoxelTorch : public UVoxel
{
	GENERATED_BODY()

public:
	UVoxelTorch();
	
	//////////////////////////////////////////////////////////////////////////
	// Defaults
public:
	virtual void LoadData(const FString& InValue) override;

	virtual FString ToData() override;

	//////////////////////////////////////////////////////////////////////////
	// Events
public:
	virtual void OnTargetHit(ACharacterBase* InTarget, const FVoxelHitResult& InHitResult) override;

	virtual void OnTargetEnter(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult) override;

	virtual void OnTargetStay(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult) override;

	virtual void OnTargetExit(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult) override;

	virtual bool OnMouseDown(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult) override;

	virtual bool OnMouseUp(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult) override;

	virtual bool OnMouseHold(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult) override;

	virtual void OnMouseHover(const FVoxelHitResult& InHitResult) override;
};
