// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ParameterNoEngineTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FGameplayTagN
{
	GENERATED_USTRUCT_BODY()

public:
	FGameplayTagN()
	{
	}

	friend bool operator==(const FGameplayTagN& A, const FGameplayTagN& B)
	{
		return false;
	}

	friend bool operator!=(const FGameplayTagN& A, const FGameplayTagN& B)
	{
		return false;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FGameplayTagContainerN
{
	GENERATED_USTRUCT_BODY()

public:
	FGameplayTagContainerN()
	{
	}

	friend bool operator==(const FGameplayTagContainerN& A, const FGameplayTagContainerN& B)
	{
		return false;
	}

	friend bool operator!=(const FGameplayTagContainerN& A, const FGameplayTagContainerN& B)
	{
		return false;
	}
};

typedef FGameplayTagN FGameplayTag;
typedef FGameplayTagContainerN FGameplayTagContainer;
