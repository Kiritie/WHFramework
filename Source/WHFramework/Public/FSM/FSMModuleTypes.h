// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "UObject/NoExportTypes.h"

#include "FSMModuleTypes.generated.h"

class UFSMComponent;

USTRUCT(BlueprintType)
struct FFSMGroupInfo
{
	GENERATED_BODY()

public:
	FFSMGroupInfo()
	{
		FSMArray = TArray<UFSMComponent*>();
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<UFSMComponent*> FSMArray;
};
