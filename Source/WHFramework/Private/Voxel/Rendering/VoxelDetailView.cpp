#include "Voxel/Rendering/VoxelDetailView.h"
#include "Voxel/Rendering/VoxelDetailData.h"
#include "Voxel/Generation/Kernel/VoxelGenMath.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Tasks/VoxelTaskScheduler.h"
#include "Voxel/Components/VoxelCollisionComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
FVoxelDetailView::FVoxelDetailView(UVoxelModule& M,FVoxelTaskScheduler& S,uint64 E):Module(M),Scheduler(S),Epoch(E)
{
    CommitHandle=M.OnBlocksCommitted.AddLambda([this](const FVoxelEditBatch&){bDirty=true;});
    RemoteHandle=M.OnRemoteBatchCompleted.AddLambda([this](const FVoxelSnapshotBatch&,bool OK){bDirty|=OK;});
}
FVoxelDetailView::~FVoxelDetailView()
{
    Reset();Module.OnBlocksCommitted.Remove(CommitHandle);Module.OnRemoteBatchCompleted.Remove(RemoteHandle);
}
bool FVoxelDetailView::Initialize(FString& Error)
{
    if(!Module.GetGenerator()||!Module.GetWorld())return false;
    const bool Render=Module.GetWorld()->GetNetMode()!=NM_DedicatedServer;
    const auto& C=*Module.GetGenerator()->GetConfig().Catalog;
    if(C.details.size()>64){Error=TEXT("At most 64 shared detail definitions per profile");return false;}
    for(const auto& D:C.details)
    {
        auto* A=LoadObject<UVoxelDetailData>(nullptr,UTF8_TO_TCHAR(D.assetPath.c_str()));
        if(!A||!A->Validate(Error)||A->GeometryHash!=D.geometryHash)
        {Error=TEXT("Detail asset/collision fingerprint mismatch");Assets.Reset();return false;}
        // Dedicated server reads only baked metadata/simple boxes; it never loads Mesh.
        if(Render&&!A->Mesh.LoadSynchronous()){Error=TEXT("Cooked detail mesh missing");Assets.Reset();return false;}
        Assets.Emplace(A); Meshes.Emplace(Render ? A->Mesh.Get() : nullptr);
    }
    return true;
}
void FVoxelDetailView::AbortStage()
{
    for(auto A:Staged)if(A.IsValid())A->Destroy();Staged.Reset();Pending.Reset();AssetCursor=0;
}
void FVoxelDetailView::Reset()
{
    bStopped=true;AbortStage();for(auto A:Active)if(A.IsValid())A->Destroy();Active.Reset();Assets.Reset();Meshes.Reset();
}
bool FVoxelDetailView::BuildBatch(uint32 Index,FString& Error)
{
    if(!Pending||!Assets.IsValidIndex(Index))return false;
    auto* W=Module.GetWorld();auto* A=Assets[Index].Get();if(!W||!A)return false;
    const bool Render=W->GetNetMode()!=NM_DedicatedServer;const double Cell=Module.BlockSize();
    TArray<FTransform> Transforms;TArray<FBox> Boxes;
    FVector Anchor=FVector::ZeroVector;bool Anchored=false;
    for(const auto& P:Pending->Placements)
    {
        if(P.asset!=Index)continue;
        // A removed supporting block suppresses derived furniture locally; no gameplay entity is created.
        // Unknown support is NOT Air. Keep the immutable layout until precise data arrives.
        FVoxelBlockState Support;
        if(Module.GetRuntime()->TryGetBlock({P.cell.x,P.cell.y,P.cell.z-1},Support))
        {
            const auto* D=Module.GetRegistry()->Find(Support.TypeId);
            if(!D||!D->bSolid)continue;
        }
        const FVector WorldLocation=FVector(P.cell.x,P.cell.y,P.cell.z)*Cell;
        if(!Anchored){Anchor=WorldLocation;Anchored=true;}
        const FTransform Transform(FRotator(0,double(P.yaw)*90.0,0),WorldLocation-Anchor);
        Transforms.Add(Transform);
        bool Near=false;
        for(const FVector& Observer:CollisionObservers)
            Near |= FVector::DistSquared(Observer,WorldLocation)<=1200.0*1200.0;
        if(A->bNearCollision && Near)
        {
            for(const FBox& B:A->CollisionBoxesCm)Boxes.Add(B.TransformBy(Transform));
            if(Boxes.Num()>16384){Error=TEXT("Detail simple-collision budget exceeded");return false;}
        }
    }
    if(Transforms.IsEmpty())return true;
    if(Transforms.Num()>2048){Error=TEXT("Detail instance batch exceeds 2048");return false;}
    FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Host=W->SpawnActor<AActor>(AActor::StaticClass(),FTransform(Anchor),Params);
    if(!Host){Error=TEXT("Detail presenter actor allocation failed");return false;}
    Host->SetReplicates(false);Host->SetActorHiddenInGame(true);Host->SetActorEnableCollision(false);Staged.Add(Host);
    auto* Root=NewObject<USceneComponent>(Host);Host->SetRootComponent(Root);Root->RegisterComponent();
    if(Render)
    {
        auto* Mesh=Meshes.IsValidIndex(Index)?Meshes[Index].Get():nullptr;if(!Mesh){Error=TEXT("Detail mesh no longer available");return false;}
        auto* Instances=NewObject<UInstancedStaticMeshComponent>(Host);
        Instances->SetupAttachment(Root);Instances->SetStaticMesh(Mesh);
        Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);Instances->SetGenerateOverlapEvents(false);
        Instances->SetCanEverAffectNavigation(false);Instances->RegisterComponent();
        Instances->AddInstances(Transforms,false,false,false);
    }
    if(!Boxes.IsEmpty())
    {
        auto* C=NewObject<UVoxelCollisionComponent>(Host);C->SetupAttachment(Root);C->RegisterComponent();
        if(!C->Apply(Boxes,1.0)){Error=TEXT("Detail collision apply failed");return false;}
    }
    return true;
}
bool FVoxelDetailView::OnTask(FVoxelTaskResult&& R)
{
    if(R.Kind!=EVoxelTaskKind::BuildDetails)return false;
    if(R.Stamp.WorldEpoch!=Epoch||R.Stamp.GenerationToken!=WaitingSerial||bStopped)return true;
    bRunning=false;
    if(!R.bSuccess||R.bCanceled||!R.Details)
    {bDirty=true;LastRequest=FPlatformTime::Seconds();if(!R.bCanceled)UE_LOG(LogTemp,Error,TEXT("Detail plan: %s"),*R.Error);return true;}
    AbortStage();Pending=R.Details;return true;
}
void FVoxelDetailView::Tick(const TArray<FVector>& Observers)
{
    if(bStopped||!Module.IsReady())return;
    CollisionObservers=Observers;
    std::set<VoxelGen::I3> Centers;
    for(const FVector& P:Observers)
    {
        FIntVector V;if(VoxelCoord::FromWorld(P,Module.BlockSize(),V))
            Centers.insert({VoxelGen::FloorDiv(V.X,16),VoxelGen::FloorDiv(V.Y,16),VoxelGen::FloorDiv(V.Z,16)});
    }
    if(Centers!=LastCenters){LastCenters=Centers;bDirty=true;}
    if(Pending)
    {
        if(bDirty){AbortStage();return;} // Do not publish a plan invalidated by movement/edit during construction.
        FString Error;
        if(AssetCursor<Assets.Num())
        {
            if(!BuildBatch(uint32(AssetCursor++),Error)){UE_LOG(LogTemp,Error,TEXT("Detail staging: %s"),*Error);AbortStage();bDirty=true;LastRequest=FPlatformTime::Seconds();}
            return;
        }
        for(auto A:Active)if(A.IsValid()){A->SetActorEnableCollision(false);A->SetActorHiddenInGame(true);A->Destroy();}
        Active=MoveTemp(Staged);
        for(auto A:Active)if(A.IsValid()){A->SetActorHiddenInGame(false);A->SetActorEnableCollision(true);}
        Pending.Reset();AssetCursor=0;
    }
    const double Now=FPlatformTime::Seconds();
    if(bRunning||!bDirty||Now-LastRequest<.5)return;
    if(Centers.size()>8){UE_LOG(LogTemp,Error,TEXT("Detail view supports eight distinct observer cells"));return;}
    const uint64 Serial=NextSerial++;auto G=Module.GetGenerator();
    FVoxelTaskRequest Q;Q.Kind=EVoxelTaskKind::BuildDetails;Q.Stamp.WorldEpoch=Epoch;Q.Stamp.GenerationToken=Serial;
    Q.ReservedBytes=8ull*1024*1024;Q.Priority=300000;
    Q.Execute=[G,Centers,Serial](const std::atomic_bool& Cancel)
    {
        FVoxelTaskResult R;auto Plan=MakeShared<FVoxelDetailPlan,ESPMode::ThreadSafe>();Plan->Serial=Serial;
        const auto& C=G->GetConfig();const auto S=C.Settings.ToKernel(C.BlockSizeCentimeters);
        VoxelGen::Terrain Terrain(S,C.Catalog->palette);std::map<VoxelGen::Id,VoxelGen::DetailPlacement> Unique;
        std::string Error;
        for(auto Center:Centers)
        {
            if(Cancel.load())return R;const VoxelGen::I3 P{Center.x*16+8,Center.y*16+8,Center.z*16+8};
            VoxelGen::Box B{{P.x-160,P.y-160,S.minZ},{P.x+160,P.y+160,S.maxZ}};
            VoxelGen::StructurePlan Structures;
            if(!VoxelGen::PlanStructures(S,*C.Catalog,Terrain,B,Structures,Error,&Cancel)){R.Error=UTF8_TO_TCHAR(Error.c_str());return R;}
            for(const auto& D:Structures.details)
            {
                const int64 DX=int64(D.cell.x)-P.x,DY=int64(D.cell.y)-P.y,DZ=int64(D.cell.z)-P.z;
                if(DX*DX+DY*DY<=160*160&&FMath::Abs(DZ)<=160)Unique[D.id]=D;
                if(Unique.size()>8192){R.Error=TEXT("Detail plan exceeds 8192 instances");return R;}
            }
        }
        for(const auto& P:Unique)Plan->Placements.push_back(P.second);
        R.Details=Plan;R.bSuccess=!Cancel.load();return R;
    };
    if(Scheduler.Enqueue(MoveTemp(Q))){WaitingSerial=Serial;bRunning=true;bDirty=false;LastRequest=Now;}
}
