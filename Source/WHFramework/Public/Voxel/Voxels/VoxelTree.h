// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Voxel.h"
#include "VoxelTree.generated.h"

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
public:
	virtual void InitTree(bool bIsRoot);
	
	virtual void BuildTree();

protected:
	UPROPERTY(VisibleAnywhere)
	bool bRoot;

	UPROPERTY(VisibleAnywhere)
	int32 TreeHeight;

	UPROPERTY(VisibleAnywhere)
	int32 LeavesHeight;

	UPROPERTY(VisibleAnywhere)
	int32 LeavesWidth;

public:
	UFUNCTION(BlueprintPure)
	bool IsRoot() const { return bRoot; }

	UFUNCTION(BlueprintCallable)
	void SetRoot(bool bInRoot) { bRoot = bInRoot; }

	UFUNCTION(BlueprintPure)
	int32 GetTreeHeight() const { return TreeHeight; }

	UFUNCTION(BlueprintCallable)
	void SetTreeHeight(int32 InTreeHeight) { TreeHeight = InTreeHeight; }

	UFUNCTION(BlueprintPure)
	int32 GetLeavesHeight() const { return LeavesHeight; }

	UFUNCTION(BlueprintCallable)
	void SetLeavesHeight(int32 InLeavesHeight) { LeavesHeight = InLeavesHeight; }

	UFUNCTION(BlueprintPure)
	int32 GetLeavesWidth() const { return LeavesWidth; }

	UFUNCTION(BlueprintCallable)
	void SetLeavesWidth(int32 InLeavesWidth) { LeavesWidth = InLeavesWidth; }
};
