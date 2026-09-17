#include "Voxel/Rendering/VoxelRenderPage.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
uint64 FVoxelRenderPage::Bytes() const{uint64 N=0;for(const auto& B:Batches)N+=B.Mesh.Bytes();return N;}
bool FVoxelRenderPage::Build(VoxelView::Key K,uint64 S,const TArray<TSharedPtr<const FVoxelLodBuiltMesh,ESPMode::ThreadSafe>>& Tiles,FVoxelRenderPage& O,FString& E,const std::atomic_bool* Cancel)
{
    if(Tiles.Num()>8){E=TEXT("A render page contains at most eight sibling tiles");return false;}
    FVoxelRenderPage P;P.Page=K;P.Serial=S;const auto Origin=K.Bounds().min;TMap<uint32,int32> Last;
    for(const auto& T:Tiles)
    {
        if(!T||!(T->Key.Parent()==K)){E=TEXT("Render page tile ownership mismatch");return false;}
        const auto B=T->Key.Bounds().min-Origin;const FVector Offset(B.x,B.y,B.z);
        for(const auto& Batch:T->Mesh.Batches)
        {
            const auto& Src=Batch.Mesh;
            if(!Src.Validate()||Src.Vertices.Num()%4||Src.Triangles.Num()!=Src.Vertices.Num()/4*6){E=TEXT("Page expects isolated quad mesh buffers");return false;}
            const uint32 Key=(uint32(Batch.Group)<<16)|Batch.Bank;
            for(int V=0;V<Src.Vertices.Num();V+=4)
            {
                if((V&4095)==0&&VoxelGen::Canceled(Cancel))return false;
                int32* Existing=Last.Find(Key);int32 Index=Existing?*Existing:INDEX_NONE;
                if(Index==INDEX_NONE||P.Batches[Index].Mesh.Vertices.Num()+4>32768)
                {FVoxelRenderBatch New;New.Group=Batch.Group;New.Bank=Batch.Bank;Index=P.Batches.Add(MoveTemp(New));Last.Add(Key,Index);}
                auto& Dst=P.Batches[Index].Mesh;const int Base=Dst.Vertices.Num();
                for(int I=0;I<4;++I){Dst.Vertices.Add(Src.Vertices[V+I]+Offset);Dst.Normals.Add(Src.Normals[V+I]);Dst.UV0.Add(Src.UV0[V+I]);Dst.UV1.Add(Src.UV1[V+I]);Dst.UV2.Add(Src.UV2[V+I]);Dst.Colors.Add(Src.Colors[V+I]);Dst.Tangents.Add(Src.Tangents[V+I]);}
                for(int I=0;I<6;++I){const int Value=Src.Triangles[V/4*6+I]-V;if(Value<0||Value>3){E=TEXT("Cross-quad page index is unsupported");return false;}Dst.Triangles.Add(Base+Value);}
                if(P.Bytes()>64ull*1024*1024){E=TEXT("Merged render page exceeds 64 MiB");return false;}
            }
        }
    }
    O=MoveTemp(P);E.Reset();return true;
}
FVoxelPagePresenter::FVoxelPagePresenter(UWorld& W,UVoxelMaterialSet& M,int32 Size):World(&W),Materials(&M),CellCm(Size){}
FVoxelPagePresenter::~FVoxelPagePresenter(){Reset();}
bool FVoxelPagePresenter::Begin(uint64 S,const TArray<TSharedPtr<const FVoxelRenderPage,ESPMode::ThreadSafe>>& P,FString& E)
{
    if(Serial||!S||!World.IsValid()||!Materials.IsValid()){E=TEXT("Presenter is busy or uninitialized");return false;}
    std::set<VoxelView::Key> Seen;uint64 Bytes=0;
    for(const auto& A:P)
    {
        if(!A||!Seen.insert(A->Page).second||A->Serial!=S){E=TEXT("Duplicate/stale page stage");return false;}
        Bytes+=A->Bytes();if(Bytes>256ull*1024*1024){E=TEXT("Visibility transaction exceeds page staging budget");return false;}
        for(const auto& B:A->Batches){const auto* Bank=Materials->FindBank(B.Group,B.Bank);if(!Bank||!Bank->Material||!B.Mesh.Validate()){E=TEXT("Page material/buffer validation failed");return false;}}
    }
    Serial=S;Pending=P;PageCursor=BatchCursor=0;return true;
}
bool FVoxelPagePresenter::TickStage(FString& E)
{
    check(IsInGameThread());if(!Serial||!World.IsValid()||!Materials.IsValid())return false;
    if(IsReady())return true;
    const auto& P=Pending[PageCursor];
    if(P->Batches.IsEmpty()){Staged[P->Page]=nullptr;++PageCursor;BatchCursor=0;return true;}
    auto& Handle=Staged[P->Page];AActor* Host=Handle.Get();
    if(!Host)
    {
        FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Host=World->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,Params);
        if(!Host){E=TEXT("Cannot create transient render page");return false;}
        Handle=Host;Host->SetReplicates(false);Host->SetActorHiddenInGame(true);Host->SetActorEnableCollision(false);
        auto* Root=NewObject<USceneComponent>(Host);Host->SetRootComponent(Root);Root->RegisterComponent();
        const auto B=P->Page.Bounds().min;Host->SetActorLocation(FVector(B.x,B.y,B.z)*CellCm);
    }
    const auto& B=P->Batches[BatchCursor];const auto* Bank=Materials->FindBank(B.Group,B.Bank);
    auto* C=NewObject<UVoxelMeshComponent>(Host);C->SetupAttachment(Host->GetRootComponent());C->SetVisibility(false);C->RegisterComponent();
    if(!C->Apply(B.Mesh,CellCm,Bank->Material)){E=TEXT("Render page upload failed");return false;}
    C->SetVisibility(true); // Host remains hidden until the complete coverage transaction is committed.
    if(++BatchCursor==P->Batches.Num()){++PageCursor;BatchCursor=0;}return true;
}
bool FVoxelPagePresenter::IsReady() const{return Serial&&PageCursor>=Pending.Num();}
bool FVoxelPagePresenter::Commit(uint64 S)
{
    check(IsInGameThread());if(S!=Serial||!IsReady())return false;
    // One GameThread publication point; nothing is made visible while its neighbors are still staging.
    for(auto& X:Staged){auto Old=Active.find(X.first);if(Old!=Active.end()&&Old->second.IsValid())Old->second->SetActorHiddenInGame(true);}
    for(auto& X:Staged)if(X.second.IsValid())X.second->SetActorHiddenInGame(false);
    for(auto& X:Staged){auto Old=Active.find(X.first);if(Old!=Active.end()&&Old->second.IsValid())Old->second->Destroy();if(X.second.IsValid())Active[X.first]=X.second;else Active.erase(X.first);}
    Staged.clear();Pending.Reset();Serial=0;PageCursor=BatchCursor=0;return true;
}
void FVoxelPagePresenter::Abort(){for(auto& X:Staged)if(X.second.IsValid())X.second->Destroy();Staged.clear();Pending.Reset();Serial=0;PageCursor=BatchCursor=0;}
void FVoxelPagePresenter::Reset(){Abort();for(auto& X:Active)if(X.second.IsValid())X.second->Destroy();Active.clear();}
