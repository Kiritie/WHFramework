// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VoxelModuleStatics.generated.h"

class UWebRequestModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UVoxelModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	// Common
	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static FPrimaryAssetId VoxelTypeToAssetID(EVoxelType InVoxelType);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static EVoxelTransparency VoxelNatureToTransparency(EVoxelNature InVoxelNature);

	//////////////////////////////////////////////////////////////////////////
	// Data
	template<class T>
	static T& GetWorldData()
	{
		return static_cast<T&>(GetWorldData());
	}
	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static FVoxelWorldSaveData& GetWorldData();

	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static EVoxelWorldMode GetWorldMode();

	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static EVoxelWorldState GetWorldState();

	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static FVoxelWorldBasicSaveData GetWorldBasicData();

	//////////////////////////////////////////////////////////////////////////
	// Index
	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static FIndex LocationToChunkIndex(FVector InLocation, bool bIgnoreZ = false);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static FVector ChunkIndexToLocation(FIndex InIndex);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static FIndex LocationToVoxelIndex(FVector InLocation, bool bIgnoreZ = false);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static FVector VoxelIndexToLocation(FIndex InIndex);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static int32 VoxelIndexToNumber(FIndex InIndex);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static FIndex NumberToVoxelIndex(int32 InNumber);

	//////////////////////////////////////////////////////////////////////////
	// Chunk
	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static AVoxelChunk* FindChunkByIndex(FIndex InIndex);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static AVoxelChunk* FindChunkByLocation(FVector InLocation);
		
	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static FVoxelItem& FindVoxelByIndex(FIndex InIndex);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static FVoxelItem& FindVoxelByLocation(FVector InLocation);

	//////////////////////////////////////////////////////////////////////////
	// Voxel
	template<class T>
	static T& GetVoxel(EVoxelType InVoxelType)
	{
		return static_cast<T&>(GetVoxel(InVoxelType));
	}
	static UVoxel& GetVoxel(EVoxelType InVoxelType);

	template<class T>
	static T& GetVoxel(const FPrimaryAssetId& InVoxelID)
	{
		return static_cast<T&>(GetVoxel(InVoxelID));
	}
	static UVoxel& GetVoxel(const FPrimaryAssetId& InVoxelID);

	template<class T>
	static T& GetVoxel(const FVoxelItem& InVoxelItem)
	{
		return static_cast<T&>(GetVoxel(InVoxelItem));
	}
	static UVoxel& GetVoxel(const FVoxelItem& InVoxelItem);
	
	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static EVoxelTerrainType GetTerrainType(FIndex InIndex);
	
	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static EVoxelType GetTerrainVoxelType(FIndex InIndex);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleStatics")
	static EVoxelType GetFoliageVoxelType(FIndex InIndex);

	//////////////////////////////////////////////////////////////////////////
	// Trace
	static bool VoxelRaycastSinge(FVector InRayStart, FVector InRayEnd, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult);

	static bool VoxelRaycastSinge(EVoxelRaycastType InRaycastType, float InDistance, const TArray<AActor*>& InIgnoreActors, FVoxelHitResult& OutHitResult);

	static bool VoxelItemTraceSingle(const FVoxelItem& InVoxelItem, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult);

	static bool VoxelAgentTraceSingle(FIndex InChunkIndex, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock = false, int32 InMaxCount = 1, bool bFromCenter = false, bool bForce = false);

	static bool VoxelAgentTraceSingle(FVector InLocation, FVector2D InRange, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bSnapToBlock = false, int32 InMaxCount = 1, bool bFromCenter = false, bool bForce = false);

	static bool VoxelAgentTraceSingle(FVector InRayStart, FVector InRayEnd, float InRadius, float InHalfHeight, const TArray<AActor*>& InIgnoreActors, FHitResult& OutHitResult, bool bCheckVoxel = false);
};
