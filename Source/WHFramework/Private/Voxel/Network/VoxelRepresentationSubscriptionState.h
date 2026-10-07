#pragma once

#include "Voxel/Runtime/VoxelChangeHierarchy.h"

// 每连接冻结编辑切点并只查询变化路径；并发请求额度不约束持续的空间订阅。
struct FVoxelRepresentationSubscriptionState
{
	FVoxelCoverageIndex Index;
	FVoxelChangeHierarchySnapshot LatestCut;
	FVoxelChangeHierarchySnapshot AcceptedCut;
	FVoxelChangeHierarchySnapshot ActiveCut;
	FVoxelCoverageLocalSnapshot Changes;
	FVoxelCoverageLocalSnapshot Local;
	FVoxelCoverageLocalSnapshot Retiring;
	FVoxelGameThreadContinuation ChangesContinuation;
	FVoxelGameThreadContinuation LocalContinuation;
	FVoxelGameThreadContinuation RetirementContinuation;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WireLease;
	TArray<uint8> PendingWire;
	FIntVector RetirementPosition = FIntVector(MAX_int32);
	int32 ChangeCursor = 0;
	int32 LocalCursor = 0;
	int32 RetirementCursor = 0;
	int32 MaximumHaloStep = 1;
	bool bActiveCut = false;
	bool bRetiring = false;
};
