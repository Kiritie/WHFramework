#pragma once
#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelLodMesher.h"
class UWorld;
class AActor;
class UVoxelMaterialSet;
struct WHFRAMEWORK_API FVoxelRenderPage
{
    VoxelView::Key Page;
    uint64 Serial=0;
    TArray<FVoxelRenderBatch> Batches;
    uint64 Bytes() const;
    static bool Build(VoxelView::Key Page,uint64 Serial,const TArray<TSharedPtr<const FVoxelLodBuiltMesh,ESPMode::ThreadSafe>>& Tiles,
                      FVoxelRenderPage& Out,FString& Error,const std::atomic_bool* Cancel=nullptr);
};
// A GameThread-only presentation owner. Every actor is transient, non-replicated and contains no world Cell truth.
class WHFRAMEWORK_API FVoxelPagePresenter
{
public:
    FVoxelPagePresenter(UWorld& World,UVoxelMaterialSet& Materials,int32 CellCm);
    ~FVoxelPagePresenter();
    bool Begin(uint64 Serial,const TArray<TSharedPtr<const FVoxelRenderPage,ESPMode::ThreadSafe>>& Pages,FString& Error);
    bool TickStage(FString& Error); // Stages at most ONE bounded material batch per call.
    bool IsReady() const;
    bool Commit(uint64 Serial);
    void Abort();
    void Reset();
private:
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UVoxelMaterialSet> Materials;
    int32 CellCm=25;
    uint64 Serial=0;
    int32 PageCursor=0,BatchCursor=0;
    TArray<TSharedPtr<const FVoxelRenderPage,ESPMode::ThreadSafe>> Pending;
    std::map<VoxelView::Key,TWeakObjectPtr<AActor>> Active,Staged;
};
