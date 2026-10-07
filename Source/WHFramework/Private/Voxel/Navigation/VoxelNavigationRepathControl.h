#pragma once

#include "Voxel/Navigation/VoxelNavigationData.h"

/** 原始路径的单次自动重规划；任务完成后才由GT消费worker写入的等待分类。 */
struct FVoxelNavigationRepathControl : TSharedFromThis<FVoxelNavigationRepathControl, ESPMode::ThreadSafe>
{
	TWeakObjectPtr<AVoxelNavigationData> Navigation;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TSharedPtr<FVoxelNavigationQueryInputs, ESPMode::ThreadSafe> ExpectedSemanticInputs;
	FNavPathWeakPtr Path;
	const FNavigationPath* PathKey = nullptr;
	FVector EndLocation = FVector::ZeroVector;
	FGuid BindingId;
	FGuid ProducerId;
	uint64 WorldEpoch = 0;
	uint32 QueryId = INVALID_NAVQUERYID;
	ENavPathUpdateType::Type Reason = ENavPathUpdateType::NavigationChanged;
	bool bWaiting = true;
	bool bRequeuePending = false;
	bool bCompletionPending = false;
	FNavPathWeakPtr CompletionPath;
	ENavigationQueryResult::Type CompletionResult = ENavigationQueryResult::Invalid;
	void OnComplete(uint32 InId, ENavigationQueryResult::Type InResult, FNavPathSharedPtr InPath)
	{
		if (auto* Data = Navigation.Get()) Data->CompleteRepath(*this, InId, InResult, MoveTemp(InPath));
	}
};
