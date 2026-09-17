#pragma once
#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "Voxel/Generation/Kernel/VoxelGenStructures.h"
#include "Voxel/Rendering/VoxelDetailData.h"
#include <set>
class UVoxelModule;
class FVoxelTaskScheduler;
class AActor;
class UStaticMesh;
struct FVoxelTaskResult;
struct WHFRAMEWORK_API FVoxelDetailPlan
{
    uint64 Serial=0;
    std::vector<VoxelGen::DetailPlacement> Placements;
    uint64 Bytes() const { return Placements.size()*sizeof(VoxelGen::DetailPlacement); }
};
// Presentation and simple near collision only. No NPC, gameplay object spawning or per-instance save protocol.
class WHFRAMEWORK_API FVoxelDetailView
{
public:
    FVoxelDetailView(UVoxelModule& Module,FVoxelTaskScheduler& Scheduler,uint64 Epoch);
    ~FVoxelDetailView();
    bool Initialize(FString& Error);
    void Tick(const TArray<FVector>& Observers);
    bool OnTask(FVoxelTaskResult&& Result);
    void Reset();
private:
    bool BuildBatch(uint32 Asset,FString& Error);
    void AbortStage();
    UVoxelModule& Module;
    FVoxelTaskScheduler& Scheduler;
    uint64 Epoch=0,NextSerial=1,WaitingSerial=0;
    bool bRunning=false,bDirty=true,bStopped=false;
    double LastRequest=-100;
    std::set<VoxelGen::I3> LastCenters;
    TArray<TStrongObjectPtr<UVoxelDetailData>> Assets;
    TArray<TStrongObjectPtr<UStaticMesh>> Meshes;
    TArray<FVector> CollisionObservers;
    TSharedPtr<const FVoxelDetailPlan,ESPMode::ThreadSafe> Pending;
    TArray<TWeakObjectPtr<AActor>> Active,Staged;
    int32 AssetCursor=0;
    FDelegateHandle CommitHandle,RemoteHandle;
};
