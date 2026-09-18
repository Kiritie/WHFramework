#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Voxel/VoxelModuleTypes.h"
#include "VoxelModuleStatics.generated.h"
class UVoxelModule;
UCLASS()
class WHFRAMEWORK_API UVoxelModuleStatics:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure,meta=(WorldContext="Context")) static UVoxelModule* GetVoxelModule(const UObject* Context);
    UFUNCTION(BlueprintPure,meta=(WorldContext="Context")) static bool IsVoxelWorldReady(const UObject* Context);
    UFUNCTION(BlueprintPure,meta=(WorldContext="Context")) static float GetVoxelWarmupProgress(const UObject* Context);
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context")) static bool CreateVoxelWorld(const UObject* Context,FVoxelGenerationSettings Settings,int32 BlockSizeCentimeters,FString& Error);
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context")) static bool GetVoxelBlock(const UObject* Context,FIntVector Index,FVoxelItem& Out);
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context")) static bool TraceVoxel(const UObject* Context,FVector Start,FVector Direction,float Distance,FVoxelHitResult& Out);
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context")) static bool ImportVoxelPrefab(const UObject* Context,const FVoxelPrefabSaveData& Prefab,FIntVector Origin,FString& Error);
    UFUNCTION(BlueprintCallable,meta=(WorldContext="Context")) static bool ExportVoxelPrefab(const UObject* Context,FIntVector Min,FIntVector Max,FVoxelPrefabSaveData& Out,FString& Error);
    static bool AreCollisionsReady(UVoxelModule& Module,const FBox& Bounds);
    static bool FindStandLocation(const UObject* Context,FVector Desired,float Radius,float HalfHeight,float SearchHeight,FVector& Out);
};
