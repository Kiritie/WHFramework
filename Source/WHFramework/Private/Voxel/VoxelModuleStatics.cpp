#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Voxels/VoxelItemBridge.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
UVoxelModule* UVoxelModuleStatics::GetVoxelModule(const UObject*C)
{return C&&GEngine?UVoxelModule::Find(GEngine->GetWorldFromContextObject(C,EGetWorldErrorMode::ReturnNull)):nullptr;}
bool UVoxelModuleStatics::IsVoxelWorldReady(const UObject*C){auto*M=GetVoxelModule(C);return M&&M->IsReady();}
float UVoxelModuleStatics::GetVoxelWarmupProgress(const UObject*C){auto*M=GetVoxelModule(C);return M?M->GetWarmupProgress():0.f;}
bool UVoxelModuleStatics::CreateVoxelWorld(const UObject*C,FVoxelGenerationSettings S,int32 Size,FString&E)
{auto*M=GetVoxelModule(C);if(!M){E=TEXT("No voxel module in this world");return false;}return M->CreateWorld(S,Size,E);}
bool UVoxelModuleStatics::GetVoxelBlock(const UObject*C,FIntVector P,FVoxelItem&O)
{auto*M=GetVoxelModule(C);FVoxelBlockState B;if(!M||!M->IsReady()||!M->GetRuntime()->TryGetBlock(P,B))return false;if(B.IsAir()){O=FVoxelItem();return true;}return FVoxelItemBridge::ToItem(*M->GetRegistry(),B,1,O);}
bool UVoxelModuleStatics::TraceVoxel(const UObject*C,FVector S,FVector D,float Range,FVoxelHitResult&O)
{
    O={};auto*M=GetVoxelModule(C);if(!M||!M->IsReady())return false;auto H=M->Trace(S,D,Range);
    O.bNeedsData=H.Status==EVoxelTraceStatus::NeedsData;if(H.Status!=EVoxelTraceStatus::Hit)return false;
    O.bHit=true;O.Index=H.Index;O.PlacementIndex=H.PlacementIndex;O.Point=H.Point;O.Normal=H.Normal;
    return FVoxelItemBridge::ToItem(*M->GetRegistry(),H.State,1,O.VoxelItem);
}
bool UVoxelModuleStatics::ImportVoxelPrefab(const UObject*C,const FVoxelPrefabSaveData&P,FIntVector O,FString&E)
{auto*M=GetVoxelModule(C);if(!M){E=TEXT("No voxel module");return false;}return M->ApplyPrefab(P,O,E);}
bool UVoxelModuleStatics::ExportVoxelPrefab(const UObject*C,FIntVector A,FIntVector B,FVoxelPrefabSaveData&O,FString&E)
{auto*M=GetVoxelModule(C);if(!M){E=TEXT("No voxel module");return false;}return M->ExportPrefab(A,B,O,E);}
bool UVoxelModuleStatics::AreCollisionsReady(UVoxelModule&M,const FBox&B)
{
    if(!M.IsReady()||!B.IsValid||B.Min.ContainsNaN()||B.Max.ContainsNaN())return false;FIntVector Lo,Hi;
    if(!VoxelCoord::FromWorld(B.Min,M.BlockSize(),Lo)||!VoxelCoord::FromWorld(B.Max,M.BlockSize(),Hi))return false;
    auto A=VoxelCoord::Section(Lo),Z=VoxelCoord::Section(Hi);int64 Count=int64(Z.X-A.X+1)*(Z.Y-A.Y+1)*(Z.Z-A.Z+1);
    if(Count<=0||Count>512)return false;const auto&Config=M.GetManifest().Settings;
    for(int32 K=A.Z;K<=Z.Z;++K)for(int32 J=A.Y;J<=Z.Y;++J)for(int32 I=A.X;I<=Z.X;++I)
    {if(int64(K)*16>=Config.MaxZ||int64(K)*16+16<=Config.MinZ)continue;const auto*S=M.GetRuntime()->Find({I,J,K});if(!S||S->Status!=EVoxelSectionStatus::DataReady||!S->bHasCollision||S->bCollisionDirty)return false;}
    return true;
}
bool UVoxelModuleStatics::FindStandLocation(const UObject*C,FVector Desired,float R,float H,float Search,FVector&O)
{
    auto*M=GetVoxelModule(C);if(!M||!FMath::IsFinite(R)||!FMath::IsFinite(H)||!FMath::IsFinite(Search)||R<=0||H<R||Search<0||Search>4096||Desired.ContainsNaN())return false;
    FVector Start=Desired+FVector(0,0,Search),End=Desired-FVector(0,0,Search);FVector Ext(R,R,H);
    if(!AreCollisionsReady(*M,FBox(End-Ext,Start+Ext)))return false;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(VoxelStand),false);FCollisionObjectQueryParams Objects(ECC_WorldStatic);FHitResult Hit;
    if(!M->GetWorld()->SweepSingleByObjectType(Hit,Start,End,FQuat::Identity,Objects,FCollisionShape::MakeCapsule(R,H),Q)||Hit.bStartPenetrating||Hit.ImpactNormal.Z<.65f)return false;
    FVector Candidate=Hit.Location+FVector(0,0,2);
    if(M->GetWorld()->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(R*.99f,H*.99f),Q))return false;
    O=Candidate;return true;
}
