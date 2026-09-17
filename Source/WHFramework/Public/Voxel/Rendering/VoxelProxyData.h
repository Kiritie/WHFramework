#pragma once
#include "CoreMinimal.h"
#include "Voxel/Rendering/Kernel/VoxelViewPlan.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Save/VoxelDeltaTypes.h"
#include "Voxel/Save/VoxelRegionStore.h"
class FVoxelWorldRuntime;
struct FVoxelRegistrySnapshot;
struct WHFRAMEWORK_API FVoxelProxyRead
{
    VoxelView::Key Key;
    uint64 Serial=0,Version=0,SaveFence=0;
    bool bRequiresSplit=false;
    FString SourceDirectory;
    TSharedPtr<const FVoxelGenerationPipeline,ESPMode::ThreadSafe> Generator;
    TSharedPtr<const FVoxelRegistrySnapshot,ESPMode::ThreadSafe> Registry;
    FVoxelWorldManifest Manifest;
    TMap<FVoxelSectionKey,FVoxelSectionOverlay> Resident;
};
enum class EVoxelProxyMode : uint8 { Natural, Grid, Retry, Split, Unchanged };
struct WHFRAMEWORK_API FVoxelProxyReply
{
    VoxelView::Key Key;
    uint64 Serial=0,Version=0;
    EVoxelProxyMode Mode=EVoxelProxyMode::Retry;
    VoxelView::Grid Grid;
};
struct WHFRAMEWORK_API FVoxelProxyRequest
{
    VoxelView::Key Key;
    uint64 Serial=0,KnownVersion=MAX_uint64;
};
class WHFRAMEWORK_API FVoxelProxyData
{
public:
    static bool Capture(VoxelView::Key Key,uint64 Serial,uint64 Version,uint64 SaveFence,
                        const FVoxelWorldManifest& Manifest,TSharedPtr<const FVoxelGenerationPipeline,ESPMode::ThreadSafe> Generator,
                        TSharedPtr<const FVoxelRegistrySnapshot,ESPMode::ThreadSafe> Registry,
                        const FVoxelRegionStore& Store,const FVoxelWorldRuntime& Runtime,FVoxelProxyRead& Out);
    static bool Build(const FVoxelProxyRead& Read,FVoxelProxyReply& Out,FString& Error,const std::atomic_bool* Cancel=nullptr);
    static bool EncodeRequest(const FVoxelProxyRequest& Request,TArray<uint8>& Out);
    static bool DecodeRequest(TConstArrayView<uint8> Bytes,FVoxelProxyRequest& Out);
    static bool EncodeReply(const FVoxelProxyReply& Reply,TArray<uint8>& Out);
    static bool DecodeReply(TConstArrayView<uint8> Bytes,uint32 SymbolCount,FVoxelProxyReply& Out);
    static bool ValidKey(VoxelView::Key Key,bool AllowFine=false);
};
