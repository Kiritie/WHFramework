// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WidgetModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static class AWidgetModule* WidgetModule;

	UFUNCTION(BlueprintPure, meta = (WorldContext = InWorldContext), Category = "WidgetModuleBPLibrary")
	static class AWidgetModule* GetWidgetModule(UObject* InWorldContext);
};