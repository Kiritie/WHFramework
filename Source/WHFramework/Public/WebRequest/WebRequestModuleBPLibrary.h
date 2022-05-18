// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WebRequestModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Parameter/ParameterModuleTypes.h"
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
	//////////////////////////////////////////////////////////////////////////
	/// WebContent
	UFUNCTION(BlueprintPure, Category = "ParameterModuleBPLibrary")
	static FString ParseWebContentToString(FParameterMap InParamMap, EWebContentType InWebContentType = EWebContentType::Form);
};
