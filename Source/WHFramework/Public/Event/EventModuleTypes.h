// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Parameter/ParameterModuleTypes.h"

#include "EventModuleTypes.generated.h"

class UEventHandleBase;

DECLARE_DELEGATE_ThreeParams(FEventHandleDelegate, TSubclassOf<UEventHandleBase>, UObject*, TArray<FParameter>);

UENUM(BlueprintType)
enum class EEventType : uint8
{
	Single,
	Multicast
};

UENUM(BlueprintType)
enum class EEventNetType : uint8
{
	Single,
	Client,
	Server,
	Multicast
};

USTRUCT(BlueprintType)
struct FEventHandleFuncs
{
	GENERATED_BODY()

public:
	FEventHandleFuncs()
	{
		FuncNames = TArray<FName>(); 
	}

	UPROPERTY()
	TArray<FName> FuncNames;
};

USTRUCT(BlueprintType)
struct FEventHandleInfo
{
	GENERATED_BODY()

public:
	FEventHandleInfo()
	{
		EventHandleFuncMap = TMap<UObject*, FEventHandleFuncs>();
	}

	FEventHandleInfo(FEventHandleDelegate InSingleDelegate)
	{
		EventHandleDelegate = InSingleDelegate;
		EventHandleFuncMap = TMap<UObject*, FEventHandleFuncs>();
	}
	
	FEventHandleDelegate EventHandleDelegate;

	UPROPERTY()
	TMap<UObject*, FEventHandleFuncs> EventHandleFuncMap;
};

USTRUCT(BlueprintType)
struct FEventHandles
{
	GENERATED_BODY()

public:
	FEventHandles()
	{
		Handles = TArray<UEventHandleBase*>();
	}

	UPROPERTY()
	TArray<UEventHandleBase*> Handles;
};
