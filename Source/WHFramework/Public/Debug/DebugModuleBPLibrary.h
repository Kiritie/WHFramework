// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DebugModuleBPLibrary.generated.h"

class ADebugModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UDebugModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static ADebugModule* DebugModuleInst;

	UFUNCTION(BlueprintPure, meta = (WorldContext = "InWorldContext"), Category = "DebugModuleBPLibrary")
	static ADebugModule* GetDebugModule(UObject* InWorldContext);
};
