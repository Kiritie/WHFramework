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
	static FPrimaryAssetId GetAssetIDByVoxelType(EVoxelType InVoxelType);

	//////////////////////////////////////////////////////////////////////////
	// Index
public:
	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FIndex LocationToChunkIndex(FVector InLocation, bool bIgnoreZ = false);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FVector ChunkIndexToLocation(FIndex InIndex);
};
