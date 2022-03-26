// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "DreamWorld/DreamWorld.h"
#include "Voxel/Voxel.h"
#include "VoxelDoor.generated.h"

class AVoxelChunk;

/**
 * ������
 */
UCLASS()
class WHFRAMEWORK_API UVoxelDoor : public UVoxel
{
	GENERATED_BODY()

public:
	UVoxelDoor();
	
	//////////////////////////////////////////////////////////////////////////
	// Defaults
public:
	virtual void LoadData(const FString& InValue) override;

	virtual FString ToData() override;

	virtual void OpenOrClose();

	virtual void OpenTheDoor();

	virtual void CloseTheDoor();

	virtual bool IsOpened() const;

	virtual void SetOpened(bool InValue);

	//////////////////////////////////////////////////////////////////////////
	// Events
public:
	virtual void OnSpawn_Implementation() override;
	
	virtual void OnTargetHit(ACharacterBase* InTarget, const FVoxelHitResult& InHitResult) override;

	virtual void OnTargetEnter(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult) override;

	virtual void OnTargetStay(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult) override;

	virtual void OnTargetExit(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult) override;

	virtual bool OnMouseDown(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult) override;

	virtual bool OnMouseUp(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult) override;

	virtual bool OnMouseHold(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult) override;

	virtual void OnMouseHover(const FVoxelHitResult& InHitResult) override;
};
