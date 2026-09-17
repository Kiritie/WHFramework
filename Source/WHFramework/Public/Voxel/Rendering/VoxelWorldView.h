#pragma once
#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelProxyData.h"
#include "Voxel/Rendering/VoxelRenderPage.h"
#include "Voxel/Rendering/Kernel/VoxelViewCoverage.h"
class UVoxelModule;
class UVoxelModuleNetworkComponent;
class FVoxelTaskScheduler;
struct FVoxelTaskResult;
class WHFRAMEWORK_API FVoxelWorldView
{
public:
    FVoxelWorldView(UVoxelModule& Module,FVoxelTaskScheduler& Scheduler,FVoxelRegionStore& Store,uint64 WorldEpoch,bool bRendering);
    ~FVoxelWorldView();
    void Tick(const TArray<FVector>& LocalObservers);
    bool OnTask(FVoxelTaskResult&& Result);
    void Invalidate(const FVoxelEditBatch& Batch);
    void InvalidateProxy(VoxelView::Key Key);
    void OnSaveBoundary(bool bBefore);
    void ReceiveProxy(const FVoxelProxyReply& Reply);
    bool ServeProxy(const FVoxelProxyRequest& Request,UVoxelModuleNetworkComponent* Peer);
    uint64 Revision(VoxelView::Key Key) const;
    void AppendFineDemand(TMap<FVoxelSectionKey,FVoxelSectionDemand>& Desired) const;
    void Reset();
private:
    struct FGrid
    {
        std::shared_ptr<const VoxelView::Grid> Data;
        uint64 Request=0,Version=MAX_uint64,FineToken=0,FineRevision=0;
        double RequestedAt=-100,UsedAt=0;
        bool Dirty=true,Pending=false;
    };
    enum class EJob : uint8 { Authority, Natural, Mesh, Page, Plan };
    struct FJob
    {
        EJob Kind=EJob::Authority;
        VoxelView::Key Key;
        uint64 PeerSerial=0,Version=0,Fence=0,CoverageSerial=0;
        TWeakObjectPtr<UVoxelModuleNetworkComponent> Peer;
        bool bRemote=false;
    };
    struct FStage
    {
        VoxelView::Change Change;
        std::map<VoxelView::Key,std::shared_ptr<const VoxelView::Grid>> Grids;
        std::map<VoxelView::Key,TSharedPtr<const FVoxelLodBuiltMesh,ESPMode::ThreadSafe>> Meshes;
        std::set<VoxelView::Key> NeedMesh,QueuedMesh,NeedPages,QueuedPages;
        std::map<VoxelView::Key,TSharedPtr<const FVoxelRenderPage,ESPMode::ThreadSafe>> Pages;
        bool bPagePlan=false,bUploading=false;
        uint64 StagingBytes=0;
    };
    bool Ready(VoxelView::Key Key);
    void Request(VoxelView::Key Key);
    bool RequestNatural(const FVoxelProxyReply& Reply);
    bool Begin(const VoxelView::Change& Change);
    void PumpStage();
    void AbortStage();
    bool Enqueue(FJob Job,TFunction<FVoxelTaskResult(const std::atomic_bool&)> Work,uint64 Reserve,uint64 Input=0);
    UVoxelModuleNetworkComponent* SelectPeer(VoxelView::Key Key) const;
    bool UpdateDesired(const TArray<FVector>& Observers);
    UVoxelModule& Module;
    FVoxelTaskScheduler& Scheduler;
    FVoxelRegionStore& Store;
    uint64 Epoch=0,NextJob=1,NextRequest=1,SaveFence=1,LatestVersion=0;
    bool bRendering=false,bSaving=false,bStopped=false,bPlanRunning=false;
    uint64 PlanSerial=0;
    double LastPlan=-100;
    VoxelView::Settings Policy;
    VoxelView::Coverage Coverage{4};
    std::vector<VoxelView::Key> DesiredLeaves;
    std::set<VoxelView::Key> Forced,DirtyVisible;
    std::map<VoxelView::Key,FGrid> Cache;
    std::map<VoxelView::Key,uint64> Versions;
    std::map<VoxelView::Key,std::shared_ptr<const VoxelView::Grid>> Displayed;
    std::map<VoxelView::Key,TSharedPtr<const FVoxelLodBuiltMesh,ESPMode::ThreadSafe>> TileMeshes;
    TMap<uint64,FJob> Jobs;
    TUniquePtr<FStage> Stage;
    TUniquePtr<FVoxelPagePresenter> Presenter;
    FDelegateHandle CommitHandle,RemoteHandle;
};
