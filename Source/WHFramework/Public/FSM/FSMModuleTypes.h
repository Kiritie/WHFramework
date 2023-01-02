// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "UObject/NoExportTypes.h"

#include "FSMModuleTypes.generated.h"

class UFSMComponent;
class UFiniteStateBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFiniteStateChanged, UFiniteStateBase*, InFiniteState);

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
