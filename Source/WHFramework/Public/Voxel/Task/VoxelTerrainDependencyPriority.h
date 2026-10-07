#pragma once

#include "CoreMinimal.h"
#include "Voxel/Task/VoxelTerrainWorkSelection.h"

// 依赖图只计算优先级捐赠，世界事实、任务及 Ready 状态仍由既有所有者保存。
class WHFRAMEWORK_API FVoxelTerrainDependencyPriority
{
public:
    void Register(const FGuid& InNode, const FVoxelTerrainServicePriority& InOwnPriority);
    bool SetDependencies(const FGuid& InNode, const TSet<FGuid>& InDependencies, FString& OutError);
    void SetPriority(const FGuid& InNode, const FVoxelTerrainServicePriority& InPriority);
    void Remove(const FGuid& InNode);
    int32 Advance(int32 InMaximumNodes, TFunctionRef<void(const FGuid&, const FVoxelTerrainServicePriority&)> InOnChanged);
    const FVoxelTerrainServicePriority* Find(const FGuid& InNode) const;
    bool IsPending() const;
    void Reset();

private:
    struct FNode
    {
        FVoxelTerrainServicePriority Own;
        FVoxelTerrainServicePriority Effective;
        TSet<FGuid> Dependencies;
        TSet<FGuid> Consumers;
    };
    bool Reaches(const FGuid& InStart, const FGuid& InTarget) const;
    void Dirty(const FGuid& InNode);
    TMap<FGuid, FNode> Nodes;
    TArray<FGuid> Queue;
    TSet<FGuid> Queued;
    int32 Cursor = 0;
};
