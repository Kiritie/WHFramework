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

	//////////////////////////////////////////////////////////////////////////
	// Asset
public:
	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FPrimaryAssetId VoxelTypeToAssetID(EVoxelType InVoxelType);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static EVoxelType AssetIDToVoxelType(FPrimaryAssetId InAssetID);

	//////////////////////////////////////////////////////////////////////////
	// Data
public:
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

	//////////////////////////////////////////////////////////////////////////
	// Index
public:
	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FIndex LocationToChunkIndex(FVector InLocation, bool bIgnoreZ = false);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FVector ChunkIndexToLocation(FIndex InIndex);

	//////////////////////////////////////////////////////////////////////////
	// Voxel
public:
	static UVoxel& GetVoxel(EVoxelType InVoxelType);

	static UVoxel& GetVoxel(const FPrimaryAssetId& InVoxelID);

	static UVoxel& GetVoxel(const FVoxelItem& InVoxelItem);

	static bool IsValid(UVoxel& InVoxel);
};
