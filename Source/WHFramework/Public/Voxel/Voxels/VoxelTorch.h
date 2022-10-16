// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Voxel.h"
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
	virtual void OnReset_Implementation() override;

	virtual bool HasArchive() const override { return true; }

	virtual void Serialize(FArchive& Ar) override;
	
	virtual void LoadData(FSaveData* InSaveData, bool bForceMode) override;

	virtual FSaveData* ToData() override;

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

	virtual void TakeOn();

	virtual void TakeOff();

	//////////////////////////////////////////////////////////////////////////
	// Stats
protected:
	UPROPERTY(VisibleAnywhere)
	bool bOn;

public:
	UFUNCTION(BlueprintPure)
	bool IsOn() const { return bOn; }
};
