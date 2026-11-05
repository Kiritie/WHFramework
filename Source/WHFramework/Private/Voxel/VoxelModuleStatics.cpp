#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Voxels/VoxelItemBridge.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
    EVoxelCollisionRangeStatus QueryVoxelCollisionRange(UVoxelModule& InModule, const FBox& InBounds,
        FIntVector& OutFailedSection, const bool bCheckFailures)
    {
        OutFailedSection = FIntVector::ZeroValue;
        if (!InModule.IsReady() || !InBounds.IsValid || InBounds.Min.ContainsNaN() || InBounds.Max.ContainsNaN())
        {
            return EVoxelCollisionRangeStatus::Pending;
        }
        const FVector MinCell = InBounds.Min / InModule.BlockSize();
        const FVector MaxCell = InBounds.Max / InModule.BlockSize();
        const FIntVector MinimumCell(FMath::FloorToInt(MinCell.X), FMath::FloorToInt(MinCell.Y), FMath::FloorToInt(MinCell.Z));
        const FIntVector MaximumCell(FMath::FloorToInt(MaxCell.X), FMath::FloorToInt(MaxCell.Y), FMath::FloorToInt(MaxCell.Z));
        const auto FloorSection = [](const int32 InCell)
        {
            return FMath::FloorToInt(static_cast<double>(InCell) / 16.0);
        };
        const FIntVector Minimum(FloorSection(MinimumCell.X), FloorSection(MinimumCell.Y), FloorSection(MinimumCell.Z));
        const FIntVector Maximum(FloorSection(MaximumCell.X), FloorSection(MaximumCell.Y), FloorSection(MaximumCell.Z));
        const int64 Count = int64(Maximum.X - Minimum.X + 1) * (Maximum.Y - Minimum.Y + 1) * (Maximum.Z - Minimum.Z + 1);
        if (Count <= 0 || Count > 512)
        {
            return EVoxelCollisionRangeStatus::Pending;
        }
        const auto& Settings = InModule.GetManifest().Settings;
        bool bPending = false;
        for (int32 Z = Minimum.Z; Z <= Maximum.Z; ++Z)
        {
            for (int32 Y = Minimum.Y; Y <= Maximum.Y; ++Y)
            {
                for (int32 X = Minimum.X; X <= Maximum.X; ++X)
                {
                    if (int64(Z) * 16 >= Settings.MaxZ || int64(Z) * 16 + 16 <= Settings.MinZ)
                    {
                        continue;
                    }
                    const FIntVector Section(X, Y, Z);
                    if (!InModule.IsCollisionReady(Section))
                    {
                        if (!bCheckFailures)
                        {
                            return EVoxelCollisionRangeStatus::Pending;
                        }
                        if (InModule.HasCurrentCollisionFailure(Section))
                        {
                            OutFailedSection = Section;
                            return EVoxelCollisionRangeStatus::Failed;
                        }
                        bPending = true;
                    }
                }
            }
        }
        return bPending ? EVoxelCollisionRangeStatus::Pending : EVoxelCollisionRangeStatus::Ready;
    }
}

UVoxelModule* UVoxelModuleStatics::GetVoxelModule(const UObject*C)
{return C&&GEngine?UVoxelModule::GetPtr(GEngine->GetWorldFromContextObject(C,EGetWorldErrorMode::ReturnNull)):nullptr;}
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
bool UVoxelModuleStatics::ImportVoxelPrefabAsset(const UObject* Context,
    const UVoxelPrefabData* Prefab, FIntVector Origin, FString& Error)
{
    UVoxelModule* Module = GetVoxelModule(Context);
    if (!Module || !Prefab)
    {
        Error = TEXT("Voxel module or prefab asset is missing");
        return false;
    }
    FVoxelPrefabSaveData Cells;
    return Prefab->DecodePlacementCells(Cells, Error) && Module->ApplyPrefab(Cells, Origin, Error);
}
bool UVoxelModuleStatics::ExportVoxelPrefab(const UObject*C,FIntVector A,FIntVector B,FVoxelPrefabSaveData&O,FString&E)
{auto*M=GetVoxelModule(C);if(!M){E=TEXT("No voxel module");return false;}return M->ExportPrefab(A,B,O,E);}
bool UVoxelModuleStatics::AreCollisionsReady(UVoxelModule&M,const FBox&B)
{
    FIntVector UnusedFailedSection;
    return QueryVoxelCollisionRange(M, B, UnusedFailedSection, false) == EVoxelCollisionRangeStatus::Ready;
}

EVoxelCollisionRangeStatus UVoxelModuleStatics::QueryCollisionReadiness(UVoxelModule& Module, const FBox& Bounds, FIntVector& OutFailedSection)
{
    return QueryVoxelCollisionRange(Module, Bounds, OutFailedSection, true);
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
