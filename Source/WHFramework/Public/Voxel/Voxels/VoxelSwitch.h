// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelInteract.h"
#include "VoxelSwitch.generated.h"

/**
 * ����ֲ��
 */
UCLASS()
class WHFRAMEWORK_API UVoxelSwitch : public UVoxelInteract
{
	GENERATED_BODY()

public:
	UVoxelSwitch();
	
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
	// Switch
public:
	virtual void Toggle(IVoxelAgentInterface* InAgent);

	virtual void Open(IVoxelAgentInterface* InAgent);

	virtual void Close(IVoxelAgentInterface* InAgent);

protected:
	UPROPERTY(VisibleAnywhere)
	bool bOpened;

public:
	bool IsOpened() const { return bOpened; }

	void SetOpened(bool bInOpened) { bOpened = bInOpened; RefreshData(); }
};
