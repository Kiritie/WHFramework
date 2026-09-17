#pragma once
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Rendering/VoxelProxyData.h"
#include <map>
struct WHFRAMEWORK_API FVoxelPhase2NetworkState
{
    TSet<FVoxelSectionKey> Fine;
    std::map<VoxelView::Key,double> Subscribed;
    std::map<VoxelView::Key,uint64> Outstanding;
    std::map<VoxelView::Key,uint64> ClientPending;
    TArray<FVoxelProxyReply> Deferred;
    double TokenTime=0,Tokens=64;
    void Reset(){Fine.Reset();Subscribed.clear();Outstanding.clear();ClientPending.clear();Deferred.Reset();TokenTime=0;Tokens=64;}
};
