// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Voxel.h"
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
	virtual void OnDespawn_Implementation() override;

	virtual void Serialize(FArchive& Ar) override;
	
	virtual void LoadData(FSaveData* InSaveData, bool bForceMode) override;

	virtual FSaveData* ToData() override;

	virtual void OpenOrClose();

	virtual void OpenTheDoor();

	virtual void CloseTheDoor();

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
	
	//////////////////////////////////////////////////////////////////////////
	// Stats
protected:
	UPROPERTY(VisibleAnywhere)
	bool bOpened;

public:
	UFUNCTION(BlueprintPure)
	bool IsOpened() const { return bOpened; }
};
