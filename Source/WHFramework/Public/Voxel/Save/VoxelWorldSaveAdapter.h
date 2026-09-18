#pragma once
#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Save/VoxelRegionStore.h"
struct WHFRAMEWORK_API FVoxelWorldSaveCapture
{
    FGuid TransactionId;
    uint64 WorldEpoch=0;
    FVoxelWorldManifest Manifest;
    FString SourceDirectory;
    TSharedPtr<const FVoxelRegistrySnapshot,ESPMode::ThreadSafe> Registry;
    TArray<FVoxelSectionOverlay> Sections;
};
class WHFRAMEWORK_API FVoxelWorldSaveAdapter
{
public:
    bool Capture(FVoxelWorldRuntime& Runtime,const FVoxelWorldManifest& Manifest,
        TSharedPtr<const FVoxelRegistrySnapshot,ESPMode::ThreadSafe> Registry,const FVoxelRegionStore& Store,FString& Error);
    const FVoxelWorldSaveCapture* GetCapture()const{return Active.IsSet()?&Active.GetValue():nullptr;}
    static bool WriteCapture(const FVoxelWorldSaveCapture& Capture,const FString& TempGenerationDirectory,FString& Error);
    void Complete(FVoxelWorldRuntime& Runtime,FVoxelRegionStore& Store,bool bSuccess,const FString& CommittedDirectory);
    bool IsBusy()const{return Active.IsSet();}
private:
    TOptional<FVoxelWorldSaveCapture> Active;
};
