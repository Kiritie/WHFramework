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
	// Index
public:
	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static EDirection InvertDirection(EDirection InDirection);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FVector DirectionToVector(EDirection InDirection, FRotator InRotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FIndex DirectionToIndex(EDirection InDirection, FRotator InRotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FIndex GetAdjacentIndex(FIndex InIndex, EDirection InDirection, FRotator InRotation = FRotator::ZeroRotator);

	UFUNCTION(BlueprintPure, Category = "VoxelModuleBPLibrary")
	static FPrimaryAssetId GetAssetIDByVoxelType(EVoxelType InVoxelType);
};
