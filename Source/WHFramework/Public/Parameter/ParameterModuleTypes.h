// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Parameter/ParameterTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"

#include "ParameterModuleTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FParameterModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FParameterModuleSaveData()
	{
		Parameters = FParameters();
	}

public:
	UPROPERTY(BlueprintReadOnly)
	FParameters Parameters;
};
