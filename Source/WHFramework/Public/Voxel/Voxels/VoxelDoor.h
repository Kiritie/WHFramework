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
	virtual void OnReset_Implementation() override;

	virtual bool HasArchive() const override { return true; }

	virtual void Serialize(FArchive& Ar) override;
	
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase = EPhase::Final) override;

	virtual FSaveData* ToData(bool bRefresh) override;

	//////////////////////////////////////////////////////////////////////////
	// Events
public:
	virtual void OnGenerate(IVoxelAgentInterface* InAgent) override;

	virtual void OnDestroy(IVoxelAgentInterface* InAgent) override;

	virtual void OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual void OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual void OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual void OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual bool OnActionTrigger(IVoxelAgentInterface* InAgent, EVoxelActionType InActionType, const FVoxelHitResult& InHitResult) override;

public:
	virtual void Toggle();

	virtual void Open();

	virtual void Close();

	//////////////////////////////////////////////////////////////////////////
	// Stats
protected:
	UPROPERTY(VisibleAnywhere)
	bool bOpened;

public:
	UFUNCTION(BlueprintPure)
	bool IsOpened() const { return bOpened; }
};
