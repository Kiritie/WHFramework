#pragma once

#include "CoreMinimal.h"
#include "Voxel/Task/VoxelTerrainWorkSelection.h"

// 图只拥有优先级捐赠、控制容量和局部脏队列，事实 Ready 仍由原所有者保存。
class WHFRAMEWORK_API FVoxelTerrainDependencyPriority
{
public:
	FVoxelTerrainDependencyPriority(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	~FVoxelTerrainDependencyPriority();
	FVoxelTerrainDependencyPriority(const FVoxelTerrainDependencyPriority&) = delete;
	FVoxelTerrainDependencyPriority& operator=(const FVoxelTerrainDependencyPriority&) = delete;
	bool Register(const FGuid& InNode, const FVoxelTerrainServicePriority& InOwnPriority, FString& OutError);
	bool SetDependencies(const FGuid& InNode, const TSet<FGuid>& InDependencies, FString& OutError);
	void SetPriority(const FGuid& InNode, const FVoxelTerrainServicePriority& InPriority);
	void Remove(const FGuid& InNode);
	int32 Advance(int32 InMaximumNodes, TFunctionRef<void(const FGuid&, const FVoxelTerrainServicePriority&)> InOnChanged);
	const FVoxelTerrainServicePriority* Find(const FGuid& InNode) const;
	bool HasConsumers(const FGuid& InNode) const;
	int32 GetDependencyCount(const FGuid& InNode) const;
	bool HasDependency(const FGuid& InNode, const FGuid& InDependency) const;
	bool IsPending() const;
	void Reset();

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FVoxelDependencyPriorityCapacityTest;
#endif
	struct FNode
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> DependenciesLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ConsumersLease;
		FVoxelTerrainServicePriority Own;
		FVoxelTerrainServicePriority Effective;
		TSet<FGuid> Dependencies;
		TSet<FGuid> Consumers;
		int32 ConsumersCapacity = 0;
		FGuid PreviousDirty;
		FGuid NextDirty;
		FGuid SearchNext;
		uint64 SearchVisited = 0;
		bool bDirty = false;
	};
	struct FStorage
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> NodesLease;
		TMap<FGuid, FNode> Nodes;
		int32 Capacity = 0;
	};
	struct FConsumerGrowth
	{
		FGuid Node;
		int32 Capacity = 0;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	};
	bool PrepareNodes(int32 InRequired, FString& OutError);
	bool Reaches(const FGuid& InStart, const FGuid& InTarget, bool& bOutReaches, FString& OutError);
	void Dirty(const FGuid& InNode);
	void UnlinkDirty(FNode& InNodeState);

	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FStorage, ESPMode::ThreadSafe> Storage;
	FGuid DirtyHead;
	FGuid DirtyTail;
	uint64 SearchSerial = 0;
};
