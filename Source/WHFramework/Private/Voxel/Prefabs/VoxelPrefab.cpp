#include "Voxel/Prefabs/VoxelPrefab.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Voxels/VoxelItemBridge.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Chunks/VoxelSectionSnapshot.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

AVoxelPrefab::AVoxelPrefab()
{
    PrimaryActorTick.bCanEverTick = false;
    if (!GetRootComponent())
    {
        SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PrefabRoot")));
    }
}

bool AVoxelPrefab::SetPreviewAsset(UVoxelPrefabData* Asset, double Size, FString& Error)
{
    if (!Asset)
    {
        Error = TEXT("Prefab asset is null");
        return false;
    }
    if (!SetPreviewData(Asset->Data, Size, Error))
    {
        return false;
    }
    PreviewAsset = Asset;
    return true;
}

bool AVoxelPrefab::SetPreviewData(const FVoxelPrefabSaveData& Value, double Size, FString& Error)
{
    check(IsInGameThread());
    Error.Reset();
    if (!GetWorld() || !FMath::IsFinite(Size) || Size <= 0.0 || Size > 10000.0)
    {
        Error = TEXT("Invalid prefab preview world or block size");
        return false;
    }
    UVoxelModule* Module = UVoxelModule::Find(GetWorld());
    const auto Registry = Module ? Module->GetRegistry() : nullptr;
    const auto Shapes = Module ? Module->GetShapes() : nullptr;
    if (!Registry || !UVoxelPrefabData::ValidateCells(Value, *Registry, Error))
    {
        if (Error.IsEmpty()) Error = TEXT("Prefab preview registry is unavailable");
        return false;
    }
    if (Value.Cells.IsEmpty() || GetNetMode() == NM_DedicatedServer)
    {
        ClearPreview();
        PreviewData = Value;
        PreviewBlockSize = Size;
        return true;
    }
    UVoxelMaterialSet* Materials = Module->GetMaterialSet();
    if (!Shapes || !Materials || !Materials->Validate(Error))
    {
        if (Error.IsEmpty()) Error = TEXT("Prefab preview rendering resources are unavailable");
        return false;
    }
    TMap<FIntVector, FVoxelBlockState> Blocks;
    TSet<FVoxelSectionKey> UniqueKeys;
    for (const FVoxelPrefabCell& Cell : Value.Cells)
    {
        FVoxelBlockState State;
        if (!FVoxelItemBridge::ToBlock(*Registry, Cell.Item, State))
        {
            Error = TEXT("Prefab block resolution failed");
            return false;
        }
        Blocks.Add(Cell.Offset, State);
        UniqueKeys.Add(VoxelCoord::Section(Cell.Offset));
    }
    auto Sample = [&Blocks](const FIntVector& Position)
    {
        const auto* Found = Blocks.Find(Position);
        return Found ? Found->Pack() : uint32(0);
    };
    TArray<FVoxelSectionKey> Keys = UniqueKeys.Array();
    Keys.Sort();
    TArray<FVoxelSectionMeshResult> Results;
    uint64 TotalBytes = 0;
    for (const FVoxelSectionKey& Key : Keys)
    {
        FVoxelSectionSnapshot Snapshot;
        Snapshot.Stamp.Key = Key;
        Snapshot.Blocks.SetNumUninitialized(VoxelBlock::Volume);
        const FIntVector Origin = VoxelCoord::Origin(Key);
        for (int32 I = 0; I < VoxelBlock::Volume; ++I)
        {
            Snapshot.Blocks[I] = Sample(Origin + VoxelCoord::Unlinear(uint16(I)));
        }
        for (uint8 Face = 0; Face < 6; ++Face)
        {
            Snapshot.Known[Face] = true;
            Snapshot.Halo[Face].SetNumUninitialized(256);
            const int32 Axis = Face / 2;
            const int32 U = (Axis + 1) % 3;
            const int32 V = (Axis + 2) % 3;
            for (int32 Y = 0; Y < 16; ++Y)
            {
                for (int32 X = 0; X < 16; ++X)
                {
                    FIntVector P = FIntVector::ZeroValue;
                    P[Axis] = (Face & 1) ? -1 : 16;
                    P[U] = X;
                    P[V] = Y;
                    Snapshot.Halo[Face][X + Y * 16] = Sample(Origin + P);
                }
            }
        }
        FVoxelSectionMeshResult Result;
        if (!FVoxelSectionMesher::Build(Snapshot, *Registry, *Shapes, Result))
        {
            Error = TEXT("Prefab meshing failed");
            return false;
        }
        TotalBytes += Result.Bytes();
        if (TotalBytes > 64ull * 1024 * 1024)
        {
            Error = TEXT("Prefab mesh exceeds 64 MiB");
            return false;
        }
        Results.Add(MoveTemp(Result));
    }
    TArray<TObjectPtr<UVoxelMeshComponent>> Created;
    auto Rollback = [this, &Created]()
    {
        for (UVoxelMeshComponent* Mesh : Created)
        {
            if (Mesh)
            {
                RemoveInstanceComponent(Mesh);
                Mesh->DestroyComponent();
            }
        }
        Created.Reset();
    };
    for (const FVoxelSectionMeshResult& Result : Results)
    {
        for (const FVoxelRenderBatch& Batch : Result.Batches)
        {
            if (Batch.Mesh.Triangles.IsEmpty()) continue;
            const FVoxelMaterialBank* Bank = Materials->FindBank(Batch.Group, Batch.Bank);
            if (!Bank || !Bank->Material)
            {
                Error = TEXT("Prefab material bank is missing");
                Rollback();
                return false;
            }
            UVoxelMeshComponent* Mesh = NewObject<UVoxelMeshComponent>(this);
            if (!Mesh)
            {
                Error = TEXT("Prefab mesh allocation failed");
                Rollback();
                return false;
            }
            Created.Add(Mesh);
            AddInstanceComponent(Mesh);
            Mesh->SetupAttachment(GetRootComponent());
            Mesh->SetVisibility(false, true);
            Mesh->SetRelativeLocation(FVector(VoxelCoord::Origin(Result.Stamp.Key)) * Size);
            if (!Mesh->Apply(Batch.Mesh, Size, Bank->Material))
            {
                Error = TEXT("Prefab mesh submission failed");
                Rollback();
                return false;
            }
            Mesh->RegisterComponent();
            if (!Mesh->IsRegistered())
            {
                Error = TEXT("Prefab component registration failed");
                Rollback();
                return false;
            }
        }
    }
    // Publish only after all new meshes exist. Failure above leaves the old preview intact.
    ClearPreview();
    PreviewMeshes = MoveTemp(Created);
    PreviewData = Value;
    PreviewBlockSize = Size;
    for (UVoxelMeshComponent* Mesh : PreviewMeshes) Mesh->SetVisibility(true, true);
    return true;
}

void AVoxelPrefab::ClearPreview()
{
    for (UVoxelMeshComponent* Mesh : PreviewMeshes)
    {
        if (Mesh)
        {
            RemoveInstanceComponent(Mesh);
            Mesh->DestroyComponent();
        }
    }
    PreviewMeshes.Reset();
    PreviewData.Cells.Reset();
    PreviewAsset = nullptr;
}

void AVoxelPrefab::OnDespawn_Implementation(EObjectDespawnMode Mode)
{
    ClearPreview();
    Super::OnDespawn_Implementation(Mode);
}

void AVoxelPrefab::LoadData(const FParameter& Value, EPhase Phase)
{
    if (!PHASEC(Phase, EPhase::Primary)) return;
    const FVoxelPrefabSaveData* Data = Value.GetPtr<FVoxelPrefabSaveData>();
    FString Error;
    if (!Data || !SetPreviewData(*Data, PreviewBlockSize, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Prefab preview load failed: %s"),
            Data ? *Error : TEXT("wrong parameter type"));
    }
}

FParameter AVoxelPrefab::ToData()
{
    return FParameter(PreviewData);
}

void AVoxelPrefab::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearPreview();
    Super::EndPlay(Reason);
}
