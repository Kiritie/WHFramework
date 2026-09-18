#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Components/VoxelCollisionComponent.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Save/VoxelSceneColumnCodec.h"
#include "SaveGame/Base/SaveDataAgentInterface.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Ability/AbilityModuleStatics.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
UWorld* UVoxelChunk::GetWorld()const{return Module?Module->GetWorld():nullptr;}
void UVoxelChunk::TickSceneActors(float){/* Base has no project actor simulation. */}
FParameter UVoxelChunk::CaptureProjectData()const{return FParameter();}
bool UVoxelChunk::RestoreProjectData(const FParameter& D,FString& E){if(D.HasValue()){E=TEXT("Unexpected project column metadata");return false;}return true;}
uint64 UVoxelChunk::MeshKey(int32 Z,EVoxelRenderGroup G,uint16 B)
{return (uint64(uint32(Z))<<24)|(uint64(uint8(G))<<16)|B;}
bool UVoxelChunk::Initialize(UVoxelModule*M,FIntPoint C)
{
    check(IsInGameThread());if(!M||!M->GetWorld()||Host)return false;Module=M;ColumnIndex=C;
    FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Host=M->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),FTransform::Identity,P);if(!Host)return false;
    Host->SetReplicates(false);auto*Root=NewObject<USceneComponent>(Host,TEXT("VoxelColumnRoot"));Host->SetRootComponent(Root);Root->RegisterComponent();
    Host->SetActorLocation(FVector(double(C.X)*16*M->BlockSize(),double(C.Y)*16*M->BlockSize(),0));return true;
}
bool UVoxelChunk::ApplyMesh(const FVoxelSectionMeshResult&R,const UVoxelMaterialSet&M,double Size)
{
    if(!Host||GetWorld()->GetNetMode()==NM_DedicatedServer)return false;TSet<uint64>Wanted;
    for(const auto&B:R.Batches)
    {
        const auto*Bank=M.FindBank(B.Group,B.Bank);if(!Bank||!Bank->Material||!B.Mesh.Validate())return false;
        uint64 K=MeshKey(R.Stamp.Key.Z,B.Group,B.Bank);Wanted.Add(K);// Host is the component owner.
        UVoxelMeshComponent*C=Meshes.FindRef(K);
        if(!C){C=NewObject<UVoxelMeshComponent>(Host);C->SetupAttachment(Host->GetRootComponent());C->SetRelativeLocation(FVector(0,0,double(R.Stamp.Key.Z)*16*Size));C->RegisterComponent();Meshes.Add(K,C);}
        if(!C->Apply(B.Mesh,Size,Bank->Material))return false;
    }
    for(auto It=Meshes.CreateIterator();It;++It)if(int32(uint32(It.Key()>>24))==R.Stamp.Key.Z&&!Wanted.Contains(It.Key()))
    {if(It.Value())It.Value()->DestroyComponent();It.RemoveCurrent();}return true;
}
bool UVoxelChunk::ApplyCollision(const FVoxelSectionCollisionResult&R,double Size)
{
    if(!Host)return false;UVoxelCollisionComponent*C=Collisions.FindRef(R.Stamp.Key.Z);
    if(!C){C=NewObject<UVoxelCollisionComponent>(Host);C->SetupAttachment(Host->GetRootComponent());C->SetRelativeLocation(FVector(0,0,double(R.Stamp.Key.Z)*16*Size));C->RegisterComponent();Collisions.Add(R.Stamp.Key.Z,C);}
    return C->Apply(R.Boxes,Size);
}
void UVoxelChunk::ClearMesh(int32 Z)
{for(auto It=Meshes.CreateIterator();It;++It)if(int32(uint32(It.Key()>>24))==Z){if(It.Value())It.Value()->DestroyComponent();It.RemoveCurrent();}}
void UVoxelChunk::ClearCollision(int32 Z)
{if(UVoxelCollisionComponent*C=Collisions.FindRef(Z))C->DestroyComponent();Collisions.Remove(Z);}
bool UVoxelChunk::ShouldPersistActor(AActor*A)const
{return IsValid(A)&&!A->IsActorBeingDestroyed()&&!(Cast<APawn>(A)&&Cast<APawn>(A)->IsPlayerControlled());}
bool UVoxelChunk::CaptureActors(TArray<uint8>&O,FString&E)const
{
    TArray<FVoxelSavedSceneActor>A;
    for(const auto&P:SceneActorMap)if(ShouldPersistActor(P.Value))
    {auto*Agent=Cast<ISaveDataAgentInterface>(P.Value);if(!Agent){E=TEXT("Scene actor does not implement SaveDataAgent");return false;}
        FVoxelSavedSceneActor S;S.Id=P.Key;S.Data=Agent->GetSaveData(true);A.Add(MoveTemp(S));}
    return FVoxelSceneColumnCodec::Encode(A,CaptureProjectData(),O,E);
}
bool UVoxelChunk::RestoreActors(TConstArrayView<uint8>B,FString&E)
{
    if(!Module||!Module->IsAuthority()||bSceneReady)return false;
    TArray<FVoxelSavedSceneActor>Records;FParameter Meta;if(!FVoxelSceneColumnCodec::Decode(B,Records,Meta,E)||!RestoreProjectData(Meta,E))return false;
    TArray<AActor*>Created;
    for(const auto&R:Records)
    {
        if(Module->FindSceneActor(R.Id)){E=TEXT("Duplicate live scene actor GUID");for(auto*A:Created)A->Destroy();return false;}
        const UScriptStruct* Type=R.Data.GetStructType();
        AActor*A=Type&&Type->IsChildOf(FPickUpSaveData::StaticStruct()) ?
            static_cast<AActor*>(UAbilityModuleStatics::SpawnAbilityPickUp(R.Data,this)) : UAbilityModuleStatics::SpawnAbilityActor(R.Data,this);
        if(A&&A->Implements<USceneActorInterface>()){RemoveSceneActor(A);ISceneActorInterface::Execute_SetActorID(A,R.Id.ToString());}
        if(!A||!A->Implements<USceneActorInterface>()||ISceneActorInterface::Execute_GetActorID(A)!=R.Id)
        {if(A)A->Destroy();for(auto*X:Created)X->Destroy();E=TEXT("Scene actor factory failed");return false;}
        A->SetReplicates(true);Created.Add(A);AddSceneActor(A);
    }
    bSceneReady=true;bSceneLoading=false;return true;
}
bool UVoxelChunk::HasSceneActor(const FString&ID,bool E)const{return GetSceneActor(ID,nullptr,E)!=nullptr;}
AActor* UVoxelChunk::GetSceneActor(const FString&ID,TSubclassOf<AActor>C,bool)const
{FGuid G;if(!FGuid::Parse(ID,G))return nullptr;AActor*A=SceneActorMap.FindRef(G);return IsValid(A)&&(!C||A->IsA(C))?A:nullptr;}
bool UVoxelChunk::AddSceneActor(AActor*A)
{
    if(!IsValid(A)||!Module||!Module->IsAuthority()||!A->Implements<USceneActorInterface>())return false;
    FGuid ID=ISceneActorInterface::Execute_GetActorID(A);if(!ID.IsValid())return false;
    if(AActor*Existing=Module->FindSceneActor(ID))if(Existing!=A)return false;
    SceneActorMap.Add(ID,A);return true;
}
bool UVoxelChunk::RemoveSceneActor(AActor*A)
{if(!A)return false;for(auto It=SceneActorMap.CreateIterator();It;++It)if(It.Value()==A){It.RemoveCurrent();return true;}return false;}
void UVoxelChunk::DestroySceneActors()
{
    TArray<TObjectPtr<AActor>>Actors;SceneActorMap.GenerateValueArray(Actors);SceneActorMap.Reset();
    for(auto&A:Actors)if(IsValid(A)&&!(Cast<APawn>(A)&&Cast<APawn>(A)->IsPlayerControlled()))A->Destroy();
}
void UVoxelChunk::OnSectionActivated(const FVoxelSectionKey&)
{
    // 基类没有项目POI业务。生成/状态由Runtime完成；项目派生类覆写此通知扫描自己的EntityKind。
}
void UVoxelChunk::Shutdown()
{
    DestroySceneActors();for(auto&P:Meshes)if(P.Value)P.Value->DestroyComponent();Meshes.Reset();
    for(auto&P:Collisions)if(P.Value)P.Value->DestroyComponent();Collisions.Reset();if(Host)Host->Destroy();Host=nullptr;Module=nullptr;
    bSceneReady=bSceneLoading=bSceneFailed=false;
}
