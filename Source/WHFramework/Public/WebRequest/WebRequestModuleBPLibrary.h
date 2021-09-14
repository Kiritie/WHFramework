// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WebRequestModuleBPLibrary.generated.h"

class AWebRequestModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWebRequestModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static AWebRequestModule* WebRequestModuleInst;

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "WebRequestModuleBPLibrary")
	static AWebRequestModule* GetWebRequestModule(UObject* InWorldContext);
};
