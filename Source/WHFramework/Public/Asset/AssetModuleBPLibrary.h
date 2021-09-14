// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AssetModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAssetModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static class AAssetModule* AssetModuleInst;

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "AssetModuleBPLibrary")
	static class AAssetModule* GetAssetModule(UObject* InWorldContext);
};
