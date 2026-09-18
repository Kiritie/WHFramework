#include "Voxel/Rendering/VoxelWorldView.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Tasks/VoxelTaskScheduler.h"
#include "Voxel/Network/VoxelModuleNetworkComponent.h"
#include "Voxel/Generation/Kernel/VoxelGenMath.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
namespace
{
bool Touches(VoxelGen::Box A,VoxelGen::Box B)
{
    for(int Axis=0;Axis<3;++Axis){int U=(Axis+1)%3,V=(Axis+2)%3;if((A.max[Axis]==B.min[Axis]||A.min[Axis]==B.max[Axis])&&A.min[U]<B.max[U]&&A.max[U]>B.min[U]&&A.min[V]<B.max[V]&&A.max[V]>B.min[V])return true;}return false;
}
FVoxelSectionKey Section(VoxelView::Key K){return {K.coordinate.x,K.coordinate.y,K.coordinate.z};}
int64 DistanceSquaredToBox(const VoxelGen::Box& B,VoxelGen::I3 P)
{
    const int64 X=P.x<B.min.x?int64(B.min.x)-P.x:P.x>=B.max.x?int64(P.x)-B.max.x+1:0;
    const int64 Y=P.y<B.min.y?int64(B.min.y)-P.y:P.y>=B.max.y?int64(P.y)-B.max.y+1:0;
    const int64 Z=P.z<B.min.z?int64(B.min.z)-P.z:P.z>=B.max.z?int64(P.z)-B.max.z+1:0;
    return X*X+Y*Y+Z*Z;
}
bool HasVisibleSurfaceEvidence(const FVoxelGenerationPipeline& G,VoxelView::Key K,const std::vector<VoxelGen::I3>& Observers)
{
    const auto B=K.Bounds();
    // A player inside/next to an underground tile is explicit evidence: keep cave exploration responsive.
    const int64 Near=int64(K.Side())*K.Side();
    for(const auto& P:Observers)if(DistanceSquaredToBox(B,P)<=Near)return true;
    int32 Above=24;
    const auto& Config=G.GetConfig();
    if(Config.Catalog)
    {
        for(const auto& T:Config.Catalog->trees)Above=FMath::Max(Above,T.maxHeight+2*T.crownRadius+8);
        for(const auto& T:Config.Catalog->structures)Above=FMath::Max(Above,(T.bounds.max.z-T.groundZ)+8);
    }
    Above=FMath::Clamp(Above,24,1024);
    const int32 Xs[3]={B.min.x,(B.min.x+B.max.x)/2,B.max.x-1};
    const int32 Ys[3]={B.min.y,(B.min.y+B.max.y)/2,B.max.y-1};
    for(int32 Y:Ys)for(int32 X:Xs)
    {
        const auto C=G.SampleColumn(X,Y);
        const int32 Top=FMath::Max(C.SurfaceZ,C.WaterZ==MIN_int32?C.SurfaceZ:C.WaterZ);
        // Keep the surface-bearing tile plus authored vegetation/structure headroom. Deep sealed volume is omitted.
        if(B.max.z>Top-8&&B.min.z<=Top+Above)return true;
    }
    return false;
}
}
FVoxelWorldView::FVoxelWorldView(UVoxelModule& M,FVoxelTaskScheduler& S,FVoxelRegionStore& R,uint64 E,bool Render):Module(M),Scheduler(S),Store(R),Epoch(E),bRendering(Render)
{
    // Initial fixed quality profile: 32 m exact, 256 m far, +/-128 m visual height.
    // 512 m is a separate measured quality preset, not an unmeasured performance promise.
    Policy.nearRadiusCm=3200;Policy.farRadiusCm=25600;Policy.verticalRadiusCm=12800;
    if(bRendering&&M.GetWorld()&&M.GetMaterialSet())Presenter=MakeUnique<FVoxelPagePresenter>(*M.GetWorld(),*M.GetMaterialSet(),int32(M.BlockSize()));
    CommitHandle=M.OnBlocksCommitted.AddRaw(this,&FVoxelWorldView::Invalidate);
    RemoteHandle=M.OnRemoteBatchCompleted.AddLambda([this](const FVoxelSnapshotBatch& B,bool OK){if(OK){FVoxelEditBatch E;for(const auto& X:B.Sections){FVoxelSectionPatch P;P.Key=X.Key;E.Sections.Add(P);}Invalidate(E);}});
}
FVoxelWorldView::~FVoxelWorldView(){Reset();Module.OnBlocksCommitted.Remove(CommitHandle);Module.OnRemoteBatchCompleted.Remove(RemoteHandle);}
void FVoxelWorldView::Reset(){bStopped=true;AbortStage();if(Presenter)Presenter->Reset();Cache.clear();Displayed.clear();TileMeshes.clear();Coverage.Clear();Jobs.Reset();}
void FVoxelWorldView::OnSaveBoundary(bool Before){bSaving=Before;++SaveFence;}
uint64 FVoxelWorldView::Revision(VoxelView::Key K) const{auto It=Versions.find(K);return It==Versions.end()?LatestVersion:It->second;}
void FVoxelWorldView::Invalidate(const FVoxelEditBatch& B)
{
    if(bStopped)return;++LatestVersion;
    for(const auto& S:B.Sections)
    {
        VoxelGen::Box Box{{S.Key.X*16,S.Key.Y*16,S.Key.Z*16},{S.Key.X*16+16,S.Key.Y*16+16,S.Key.Z*16+16}};
        for(uint8 L=1;L<=4;++L){const int D=1<<L;VoxelView::Key K{{VoxelGen::FloorDiv(S.Key.X,D),VoxelGen::FloorDiv(S.Key.Y,D),VoxelGen::FloorDiv(S.Key.Z,D)},L};Versions[K]=LatestVersion;InvalidateProxy(K);}
        InvalidateProxy({{S.Key.X,S.Key.Y,S.Key.Z},0});
        if(Stage)for(const auto& K:Stage->Change.remesh)if(K.Bounds().Intersects(Box)){AbortStage();break;}
    }
    // Transient versions are conservative. Unknown entries use LatestVersion, never a reset-to-zero proof.
    if(Versions.size()>32768)Versions.clear();
}
void FVoxelWorldView::InvalidateProxy(VoxelView::Key K)
{
    if(auto It=Cache.find(K);It!=Cache.end()){It->second.Dirty=true;It->second.Pending=false;It->second.Request=NextRequest++;}
    if(Coverage.Visible().count(K))DirtyVisible.insert(K);
    if(Stage&&(Stage->Grids.count(K)))AbortStage();
}
bool FVoxelWorldView::Enqueue(FJob J,TFunction<FVoxelTaskResult(const std::atomic_bool&)> Work,uint64 Reserve,uint64 Input)
{
    if(bStopped||Jobs.Num()>=128||NextJob==MAX_uint64)return false;
    const uint64 ID=NextJob++;FVoxelTaskRequest Q;Q.Stamp.WorldEpoch=Epoch;Q.Stamp.GenerationToken=ID;Q.Stamp.Key=Section(J.Key);
    Q.Kind=(J.Kind==EJob::Plan)?EVoxelTaskKind::BuildViewPlan:(J.Kind==EJob::Mesh)?EVoxelTaskKind::BuildViewMesh:(J.Kind==EJob::Page)?EVoxelTaskKind::BuildViewPage:EVoxelTaskKind::BuildProxy;
    Q.Priority=J.Kind==EJob::Plan?-250000:J.Kind==EJob::Authority?100000:200000+J.Key.level*1000;Q.ReservedBytes=Reserve;Q.InputBytes=Input;
    const auto Stamp=Q.Stamp;const auto Kind=Q.Kind;
    Q.Execute=[Work=MoveTemp(Work),Stamp,Kind](const std::atomic_bool& C){auto R=Work(C);R.Stamp=Stamp;R.Kind=Kind;R.bCanceled|=C.load();return R;};
    if(!Scheduler.Enqueue(MoveTemp(Q)))return false;Jobs.Add(ID,J);return true;
}
bool FVoxelWorldView::ServeProxy(const FVoxelProxyRequest& Q,UVoxelModuleNetworkComponent* Peer)
{
    if(!Module.IsAuthority()||bSaving||bStopped||!FVoxelProxyData::ValidKey(Q.Key))return false;
    const uint64 V=Revision(Q.Key);Versions[Q.Key]=V;
    if(Q.KnownVersion==V)
    {
        FVoxelProxyReply Reply;Reply.Key=Q.Key;Reply.Serial=Q.Serial;Reply.Version=V;Reply.Mode=EVoxelProxyMode::Unchanged;
        if(Peer)return Peer->DeliverProxy(Reply);ReceiveProxy(Reply);return true;
    }
    FVoxelProxyRead Read;if(!FVoxelProxyData::Capture(Q.Key,Q.Serial,V,SaveFence,Module.GetManifest(),Module.GetGenerator(),Module.GetRegistry(),Store,*Module.GetRuntime(),Read))return false;
    FJob J;J.Kind=EJob::Authority;J.Key=Q.Key;J.PeerSerial=Q.Serial;J.Version=V;J.Fence=SaveFence;J.Peer=Peer;J.bRemote=Peer!=nullptr;
    uint64 InputBytes=0;for(const auto& X:Read.Resident)InputBytes+=uint64(X.Value.Blocks.Num())*48;
    return Enqueue(J,[Read=MoveTemp(Read)](const std::atomic_bool& C){FVoxelTaskResult R;R.Proxy=MakeShared<FVoxelProxyReply,ESPMode::ThreadSafe>();R.bSuccess=FVoxelProxyData::Build(Read,*R.Proxy,R.Error,&C);return R;},96ull*1024*1024,InputBytes);
}
UVoxelModuleNetworkComponent* FVoxelWorldView::SelectPeer(VoxelView::Key K) const
{
    UWorld* W=Module.GetWorld();if(!W)return nullptr;const auto B=K.Bounds();const FVector Center=FVector((B.min.x+B.max.x)*.5,(B.min.y+B.max.y)*.5,(B.min.z+B.max.z)*.5)*Module.BlockSize();
    double Best=DBL_MAX;UVoxelModuleNetworkComponent* Result=nullptr;
    for(auto It=W->GetPlayerControllerIterator();It;++It)if(auto* PC=It->Get())if(PC->IsLocalController())
    {auto* N=PC->FindComponentByClass<UVoxelModuleNetworkComponent>();FVector P;FRotator R;PC->GetPlayerViewPoint(P,R);const double D=FVector::DistSquared(P,Center);if(N&&N->IsSessionReady()&&D<Best){Best=D;Result=N;}}
    return Result;
}
bool FVoxelWorldView::Ready(VoxelView::Key K)
{
    auto It=Cache.find(K);return It!=Cache.end()&&It->second.Data&&!It->second.Dirty;
}
void FVoxelWorldView::Request(VoxelView::Key K)
{
    if(!bRendering||!FVoxelProxyData::ValidKey(K,true))return;
    if(!Cache.count(K)&&Cache.size()>=32768){UE_LOG(LogTemp,Error,TEXT("View grid cache capacity exceeded"));return;}
    auto& Entry=Cache[K];const double Now=FPlatformTime::Seconds();Entry.UsedAt=Now;
    if(!K.level)
    {
        auto* S=Module.GetRuntime()->Find(Section(K));const auto& C=Module.GetGenerator()->GetConfig();const auto B=K.Bounds();
        const bool Outside=B.max.z<=C.Settings.MinZ||B.min.z>=C.Settings.MaxZ;
        if(!Outside&&(!S||S->Status!=EVoxelSectionStatus::DataReady))return;
        if(Ready(K)&&(Outside||(Entry.FineToken==S->Stamp.GenerationToken&&Entry.FineRevision==S->Stamp.Revision)))return;
        auto Grid=std::make_shared<VoxelView::Grid>();Grid->key=K;Grid->version=NextRequest++;
        if(!Outside)for(uint16 I=0;I<4096;++I)if(!Module.GetGenerator()->ToSymbol(S->Blocks.Get(I),Grid->cells[I])){UE_LOG(LogTemp,Error,TEXT("Fine view catalog binding failed"));return;}
        Entry.Data=Grid;Entry.Dirty=false;Entry.Pending=false;Entry.FineToken=Outside?0:S->Stamp.GenerationToken;Entry.FineRevision=Outside?0:S->Stamp.Revision;
        if(Coverage.Visible().count(K))DirtyVisible.insert(K);return;
    }
    if(Now<Entry.RequestedAt)return;
    if(!Entry.Dirty&&Entry.Data)return;
    if(Entry.Pending&&Now-Entry.RequestedAt<15)return;
    FVoxelProxyRequest Q;Q.Key=K;Q.Serial=NextRequest++;Q.KnownVersion=Entry.Dirty?MAX_uint64:Entry.Version;
    Entry.Request=Q.Serial;Entry.RequestedAt=Now;Entry.Pending=true;
    const bool Sent=Module.IsAuthority()?ServeProxy(Q,nullptr):(SelectPeer(K)&&SelectPeer(K)->RequestProxy(Q));
    if(!Sent){Entry.Pending=false;Entry.RequestedAt=Now+.25;}
}
bool FVoxelWorldView::RequestNatural(const FVoxelProxyReply& Reply)
{
    auto G=Module.GetGenerator();FJob J;J.Kind=EJob::Natural;J.Key=Reply.Key;J.PeerSerial=Reply.Serial;J.Version=Reply.Version;
    return Enqueue(J,[G,Reply](const std::atomic_bool& C){FVoxelTaskResult R;auto Grid=std::make_shared<VoxelView::Grid>();std::string E;const auto& Config=G->GetConfig();R.bSuccess=VoxelView::BuildProxy(Config.Settings.ToKernel(Config.BlockSizeCentimeters),Config.Catalog,Reply.Key,{},*Grid,E,&C);Grid->version=Reply.Version;R.ViewGrid=Grid;R.Error=UTF8_TO_TCHAR(E.c_str());return R;},32ull*1024*1024);
}
void FVoxelWorldView::ReceiveProxy(const FVoxelProxyReply& R)
{
    auto It=Cache.find(R.Key);if(It==Cache.end()||It->second.Request!=R.Serial||bStopped)return;auto& E=It->second;
    if(R.Mode==EVoxelProxyMode::Retry){E.Pending=false;E.RequestedAt=FPlatformTime::Seconds()+.5;return;}
    if(R.Mode==EVoxelProxyMode::Split)
    {
        E.Pending=false;
        if(R.Key.level>1){Forced.insert(R.Key);Coverage.BlockParent(R.Key);LastPlan=-100;}else UE_LOG(LogTemp,Error,TEXT("Proxy L1 unexpectedly exceeds bounded read budget"));
        return;
    }
    if(R.Mode==EVoxelProxyMode::Unchanged){E.Pending=false;if(!E.Data){E.Dirty=true;return;}E.Version=R.Version;return;}
    if(R.Mode==EVoxelProxyMode::Natural){E.Version=R.Version;if(!RequestNatural(R))E.Pending=false;return;}
    E.Data=std::make_shared<VoxelView::Grid>(R.Grid);E.Version=R.Version;E.Dirty=false;E.Pending=false;
    if(Coverage.Visible().count(R.Key))DirtyVisible.insert(R.Key);
}
bool FVoxelWorldView::UpdateDesired(const TArray<FVector>& Observers)
{
    if(bPlanRunning)return true;
    std::vector<VoxelGen::I3> Points;for(const auto& P:Observers){FIntVector V;if(VoxelCoord::FromWorld(P,Module.BlockSize(),V))Points.push_back({V.X,V.Y,V.Z});}
    if(Points.empty()){if(!DesiredLeaves.empty()){AbortStage();DesiredLeaves.clear();Coverage.SetDesired({});}return true;}
    if(Points.size()>4){UE_LOG(LogTemp,Error,TEXT("At most four local view sources are supported by this quality profile"));return false;}
    auto Generator=Module.GetGenerator();auto World=Generator->GetConfig().Settings.ToKernel(int32(Module.BlockSize()));auto View=Policy;auto Force=Forced;
    FJob J;J.Kind=EJob::Plan;J.PeerSerial=++PlanSerial;
    if(!Enqueue(J,[Generator,World,View,Force=std::move(Force),Points=std::move(Points)](const std::atomic_bool& C)
    {
        FVoxelTaskResult R;if(C.load())return R;auto Out=std::make_shared<std::vector<VoxelView::Key>>();std::string E;
        if(!VoxelView::SelectMany(View,World,Points,Force,*Out,E)){R.Error=UTF8_TO_TCHAR(E.c_str());return R;}
        Out->erase(std::remove_if(Out->begin(),Out->end(),[&](const VoxelView::Key& K){return !HasVisibleSurfaceEvidence(*Generator,K,Points);}),Out->end());
        R.bSuccess=true;R.ViewPlan=Out;R.Error.Reset();return R;
    },8ull*1024*1024))return false;
    bPlanRunning=true;return true;
}
void FVoxelWorldView::AppendFineDemand(TMap<FVoxelSectionKey,FVoxelSectionDemand>& D) const
{
    auto Add=[&](VoxelView::Key K){if(K.level)return;auto& V=D.FindOrAdd(Section(K));V.bMesh=false;V.Priority=FMath::Min(V.Priority,-500.0);};
    for(const auto& K:DesiredLeaves)Add(K);
    for(const auto& K:Coverage.Visible())if(K.level==1)for(const auto& Child:K.Children())if(Coverage.Wants(Child))Add(Child);
}
bool FVoxelWorldView::Begin(const VoxelView::Change& C)
{
    if(Stage||!Presenter)return false;auto S=MakeUnique<FStage>();S->Change=C;S->Grids=Displayed;S->Meshes=TileMeshes;
    for(const auto& K:C.remove){S->Grids.erase(K);S->Meshes.erase(K);}
    for(const auto& K:C.add){auto It=Cache.find(K);if(It==Cache.end()||!It->second.Data||It->second.Dirty)return false;S->Grids[K]=It->second.Data;}
    S->NeedMesh.insert(C.remesh.begin(),C.remesh.end());Stage=MoveTemp(S);return true;
}
void FVoxelWorldView::AbortStage(){if(Presenter)Presenter->Abort();Stage.Reset();}
void FVoxelWorldView::PumpStage()
{
    if(!Stage)return;
    if(Stage->Change.generation!=Coverage.Generation()){AbortStage();return;}
    FString E;const uint64 Serial=Stage->Change.serial;
    if(!Stage->NeedMesh.empty())
    {
        int Submitted=0;
        for(const auto& K:Stage->NeedMesh)
        {
            if(Submitted>=2)break;if(Stage->QueuedMesh.count(K))continue;
            FVoxelLodMeshInput I;I.Current=Stage->Grids.at(K);I.CoverageSerial=Serial;
            for(const auto& N:Stage->Grids)if(!(N.first==K)&&Touches(K.Bounds(),N.first.Bounds()))I.Neighbors.push_back(N.second);
            FJob J;J.Kind=EJob::Mesh;J.Key=K;J.CoverageSerial=Serial;auto G=Module.GetGenerator();auto R=Module.GetRegistry();auto H=Module.GetShapes();
            const uint64 Input=(I.Neighbors.size()+1)*16384;
            if(Enqueue(J,[I=MoveTemp(I),G,R,H](const std::atomic_bool& C){FVoxelTaskResult O;O.ViewMesh=MakeShared<FVoxelLodBuiltMesh,ESPMode::ThreadSafe>();O.bSuccess=FVoxelLodMesher::Build(I,*G,*R,*H,*O.ViewMesh,O.Error,&C);return O;},48ull*1024*1024,Input)){Stage->QueuedMesh.insert(K);++Submitted;}
        }
        return;
    }
    if(!Stage->bPagePlan)
    {
        for(const auto& K:Stage->Change.remove)Stage->NeedPages.insert(K.Parent());
        for(const auto& K:Stage->Change.remesh)Stage->NeedPages.insert(K.Parent());Stage->bPagePlan=true;
    }
    if(!Stage->NeedPages.empty())
    {
        int Submitted=0;
        for(const auto& K:Stage->NeedPages)
        {
            if(Submitted>=1)break;if(Stage->QueuedPages.count(K))continue;
            TArray<TSharedPtr<const FVoxelLodBuiltMesh,ESPMode::ThreadSafe>> Tiles;uint64 Bytes=0;
            for(const auto& P:Stage->Meshes)if(P.first.Parent()==K){Tiles.Add(P.second);Bytes+=P.second->Bytes();}
            if(Bytes>32ull*1024*1024){UE_LOG(LogTemp,Error,TEXT("View page inputs exceed 32 MiB; retain previous coverage"));AbortStage();return;}
            FJob J;J.Kind=EJob::Page;J.Key=K;J.CoverageSerial=Serial;
            if(Enqueue(J,[K,Serial,Tiles=MoveTemp(Tiles)](const std::atomic_bool& C){FVoxelTaskResult O;O.ViewPage=MakeShared<FVoxelRenderPage,ESPMode::ThreadSafe>();O.bSuccess=FVoxelRenderPage::Build(K,Serial,Tiles,*O.ViewPage,O.Error,&C);return O;},64ull*1024*1024,Bytes)){Stage->QueuedPages.insert(K);++Submitted;}
        }
        return;
    }
    if(!Stage->bUploading)
    {
        TArray<TSharedPtr<const FVoxelRenderPage,ESPMode::ThreadSafe>> Pages;for(const auto& P:Stage->Pages)Pages.Add(P.second);
        if(!Presenter->Begin(Serial,Pages,E)){UE_LOG(LogTemp,Error,TEXT("View stage: %s"),*E);AbortStage();return;}Stage->bUploading=true;
    }
    if(!Presenter->TickStage(E)){UE_LOG(LogTemp,Error,TEXT("View upload: %s"),*E);AbortStage();return;}
    if(Presenter->IsReady())
    {
        if(!Coverage.Commit(Stage->Change)){AbortStage();return;}
        const bool OK=Presenter->Commit(Serial);check(OK);
        Displayed=std::move(Stage->Grids);TileMeshes=std::move(Stage->Meshes);
        for(const auto& K:Stage->Change.add)DirtyVisible.erase(K);for(const auto& K:Stage->Change.remove)DirtyVisible.erase(K);Stage.Reset();
    }
}
bool FVoxelWorldView::OnTask(FVoxelTaskResult&& R)
{
    if(R.Kind!=EVoxelTaskKind::BuildProxy&&R.Kind!=EVoxelTaskKind::BuildViewMesh&&R.Kind!=EVoxelTaskKind::BuildViewPage&&R.Kind!=EVoxelTaskKind::BuildViewPlan)return false;
    FJob J;if(!Jobs.RemoveAndCopyValue(R.Stamp.GenerationToken,J)||R.Stamp.WorldEpoch!=Epoch||bStopped)return true;
    bool OK=R.bSuccess&&!R.bCanceled;
    if(J.Kind==EJob::Plan)
    {
        bPlanRunning=false;
        if(OK&&R.ViewPlan&&J.PeerSerial==PlanSerial&&*R.ViewPlan!=DesiredLeaves)
        {AbortStage();DesiredLeaves=*R.ViewPlan;Coverage.SetDesired(DesiredLeaves);Module.ForceVoxelStreamingRefresh();}
        else if(!OK&&!R.bCanceled)UE_LOG(LogTemp,Error,TEXT("View plan: %s"),*R.Error);
        return true;
    }
    if(J.Kind==EJob::Authority)
    {
        OK=OK&&!bSaving&&J.Fence==SaveFence&&J.Version==Revision(J.Key);
        FVoxelProxyReply Reply;if(OK&&R.Proxy)Reply=*R.Proxy;else{Reply.Key=J.Key;Reply.Serial=J.PeerSerial;Reply.Version=Revision(J.Key);Reply.Mode=EVoxelProxyMode::Retry;}
        if(J.bRemote){if(J.Peer.IsValid())J.Peer->DeliverProxy(Reply);}else ReceiveProxy(Reply);return true;
    }
    if(J.Kind==EJob::Natural)
    {
        auto It=Cache.find(J.Key);if(It==Cache.end()||It->second.Request!=J.PeerSerial)return true;auto& E=It->second;E.Pending=false;
        if(OK&&R.ViewGrid){E.Data=R.ViewGrid;E.Version=J.Version;E.Dirty=false;if(Coverage.Visible().count(J.Key))DirtyVisible.insert(J.Key);}return true;
    }
    if(!Stage||Stage->Change.serial!=J.CoverageSerial)return true;
    if(!OK){UE_LOG(LogTemp,Error,TEXT("View worker: %s"),*R.Error);AbortStage();return true;}
    if(J.Kind==EJob::Mesh&&R.ViewMesh){Stage->StagingBytes+=R.ViewMesh->Bytes();Stage->Meshes[J.Key]=R.ViewMesh;Stage->NeedMesh.erase(J.Key);}
    if(J.Kind==EJob::Page&&R.ViewPage){Stage->StagingBytes+=R.ViewPage->Bytes();Stage->Pages[J.Key]=R.ViewPage;Stage->NeedPages.erase(J.Key);}
    uint64 LiveMeshBytes=0;if(Stage)for(const auto& X:Stage->Meshes)LiveMeshBytes+=X.second->Bytes();
    if(Stage&&(Stage->StagingBytes>256ull*1024*1024||LiveMeshBytes>512ull*1024*1024)){UE_LOG(LogTemp,Error,TEXT("View transaction memory budget exceeded"));AbortStage();}return true;
}
void FVoxelWorldView::Tick(const TArray<FVector>& Observers)
{
    if(bStopped||!bRendering||!Presenter||!Module.IsReady())return;
    const double Now=FPlatformTime::Seconds();
    if(Now-LastPlan>=.2){UpdateDesired(Observers);LastPlan=Now;}
    int Renew=0;
    for(const auto& K:Coverage.Visible())
    {
        auto It=Cache.find(K);if(It!=Cache.end())It->second.UsedAt=Now;
        if(Renew<8&&(It==Cache.end()||It->second.Dirty)){Request(K);++Renew;}
    }
    if(Stage){PumpStage();return;}
    for(auto It=DirtyVisible.begin();It!=DirtyVisible.end();)
    {
        auto K=*It;if(!Coverage.Visible().count(K)){It=DirtyVisible.erase(It);continue;}
        Request(K);if(Ready(K)){VoxelView::Change C;if(Coverage.Replacement(K,C)){Begin(C);PumpStage();return;}}++It;
    }
    VoxelView::Change Change;std::vector<VoxelView::Key> Need;
    if(Coverage.Propose([this](auto K){return Ready(K);},Need,Change))Begin(Change);
    // Parent-first priority; scan bounded requests, not every possible fine section in the far radius.
    std::stable_sort(Need.begin(),Need.end(),[](const auto& A,const auto& B){return A.level>B.level;});
    for(size_t I=0;I<std::min<size_t>(Need.size(),8);++I)Request(Need[I]);
    int Evicted=0;
    for(auto It=Cache.begin();It!=Cache.end()&&Evicted<64;)
    {if(!Coverage.Wants(It->first)&&!Coverage.Visible().count(It->first)&&!It->second.Pending&&Now-It->second.UsedAt>2){It=Cache.erase(It);++Evicted;}else ++It;}
    PumpStage();
}
