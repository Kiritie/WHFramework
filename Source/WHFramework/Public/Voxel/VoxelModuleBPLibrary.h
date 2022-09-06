// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/MathTypes.h"
#include "VoxelModuleBPLibrary.generated.h"

class AWebRequestModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UVoxelModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// Asset
	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FPrimaryAssetId VoxelTypeToAssetID(EVoxelType InVoxelType);

	//////////////////////////////////////////////////////////////////////////
	// Data
	template<class T>
	static T& GetWorldData()
	{
		return static_cast<T&>(GetWorldData());
	}
	static FVoxelWorldSaveData& GetWorldData();

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static EVoxelWorldMode GetWorldMode();

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static EVoxelWorldState GetWorldState();

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FVoxelWorldBasicSaveData GetWorldBasicData();
	
	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static float GetWorldLength();

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static bool IsBasicGenerated();

	//////////////////////////////////////////////////////////////////////////
	// Index
	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FIndex LocationToChunkIndex(FVector InLocation, bool bIgnoreZ = false);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FVector ChunkIndexToLocation(FIndex InIndex);

	//////////////////////////////////////////////////////////////////////////
	// Chunk
	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static AVoxelChunk* FindChunkByIndex(FIndex InIndex);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static AVoxelChunk* FindChunkByLocation(FVector InLocation);

	//////////////////////////////////////////////////////////////////////////
	// Voxel
	static UVoxel& GetVoxel(EVoxelType InVoxelType);

	static UVoxel& GetVoxel(const FPrimaryAssetId& InVoxelID);

	static UVoxel& GetVoxel(const FVoxelItem& InVoxelItem);
	
	//////////////////////////////////////////////////////////////////////////
	// Noise
	static EVoxelType GetNoiseVoxelType(FIndex InIndex);

	//////////////////////////////////////////////////////////////////////////
	// Trace
	static bool ChunkTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);

	static bool ChunkTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);

	static bool VoxelTraceSingle(const FVoxelItem& InVoxelItem, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);
		
	static bool VoxelRaycastSinge(FVector InRayStart, FVector InRayEnd, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult);

	static bool VoxelRaycastSinge(float InDistance, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult);

	static ECollisionChannel GetChunkTraceType();

	static ECollisionChannel GetVoxelTraceType();
};
