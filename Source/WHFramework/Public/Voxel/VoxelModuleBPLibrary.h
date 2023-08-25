// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

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
	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FVoxelWorldSaveData& GetWorldData();

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static EVoxelWorldMode GetWorldMode();

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static EVoxelWorldState GetWorldState();

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FVoxelWorldBasicSaveData GetWorldBasicData();

	//////////////////////////////////////////////////////////////////////////
	// Index
	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FIndex LocationToChunkIndex(FVector InLocation, bool bIgnoreZ = false);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FVector ChunkIndexToLocation(FIndex InIndex);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FIndex LocationToVoxelIndex(FVector InLocation, bool bIgnoreZ = false);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FVector VoxelIndexToLocation(FIndex InIndex);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static int32 VoxelIndexToNumber(FIndex InIndex);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FIndex NumberToVoxelIndex(int32 InNumber);

	//////////////////////////////////////////////////////////////////////////
	// Chunk
	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static AVoxelChunk* FindChunkByIndex(FIndex InIndex);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static AVoxelChunk* FindChunkByLocation(FVector InLocation);
		
	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FVoxelItem& FindVoxelByIndex(FIndex InIndex);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FVoxelItem& FindVoxelByLocation(FVector InLocation);

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
	static bool VoxelRaycastSinge(FVector InRayStart, FVector InRayEnd, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult);

	static bool VoxelRaycastSinge(EVoxelRaycastType InRaycastType, float InDistance, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult);

	static bool VoxelItemTraceSingle(const FVoxelItem& InVoxelItem, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);

	static bool VoxelAgentTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock = false, int32 InMaxCount = 1, bool bFromCenter = false);

	static bool VoxelAgentTraceSingle(FVector InLocation, FVector2D InRange, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock = false, int32 InMaxCount = 1, bool bFromCenter = false);

	static bool VoxelAgentTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);
};
