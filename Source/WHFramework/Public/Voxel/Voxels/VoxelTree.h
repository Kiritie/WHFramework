// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Voxel.h"
#include "VoxelTree.generated.h"

class UVoxelTreeData;

/**
 * ����ֲ��
 */
UCLASS()
class WHFRAMEWORK_API UVoxelTree : public UVoxel
{
	GENERATED_BODY()

public:
	UVoxelTree();
	
	//////////////////////////////////////////////////////////////////////////
	// Voxel
public:
	virtual void OnReset_Implementation() override;
	
	virtual void LoadData(const FString& InData) override;

	virtual FString ToData() override;

	virtual FString ToData(EVoxelTreePart InTreePart, const UVoxelTreeData& InTreeData);

public:
	virtual void OnGenerate(IVoxelAgentInterface* InAgent) override;

	virtual void OnDestroy(IVoxelAgentInterface* InAgent) override;

	virtual void OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual void OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual void OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual void OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult) override;

	virtual bool OnAgentAction(IVoxelAgentInterface* InAgent, EVoxelActionType InActionType, const FVoxelHitResult& InHitResult) override;

	//////////////////////////////////////////////////////////////////////////
	// Tree
protected:
	UPROPERTY(VisibleAnywhere)
	EVoxelTreePart TreePart;

	UPROPERTY(VisibleAnywhere)
	int32 TreeHeight;

	UPROPERTY(VisibleAnywhere)
	int32 LeavesHeight;

	UPROPERTY(VisibleAnywhere)
	int32 LeavesWidth;

public:
	UFUNCTION(BlueprintPure)
	EVoxelTreePart GetTreePart() const { return TreePart; }
};
