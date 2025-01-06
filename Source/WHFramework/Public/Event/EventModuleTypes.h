// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"
#include "Parameter/ParameterModuleTypes.h"

#include "EventModuleTypes.generated.h"

class UEventHandleBase;

DECLARE_DELEGATE_FourParams(FEventHandleDelegate, TSubclassOf<UEventHandleBase>, UObject*, TArray<FParameter>, bool);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FEventExecuteDynamicDelegate, UObject*, InSender, UEventHandleBase*, InEventHandle);

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
struct FEventFuncs
{
	GENERATED_BODY()

public:
	FEventFuncs()
	{
		FuncNames = TArray<FName>(); 
	}

	UPROPERTY()
	TArray<FName> FuncNames;
};

USTRUCT(BlueprintType)
struct FEventMapping
{
	GENERATED_BODY()

public:
	FEventMapping()
	{
		FuncMap = TMap<UObject*, FEventFuncs>();
	}

	FEventMapping(const FEventHandleDelegate& InDelegate)
	{
		Delegate = InDelegate;
		FuncMap = TMap<UObject*, FEventFuncs>();
	}
	
	FEventHandleDelegate Delegate;

	UPROPERTY()
	TMap<UObject*, FEventFuncs> FuncMap;
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

USTRUCT(Blueprintable)
struct FEventInfo
{
	GENERATED_BODY()

public:
	FEventInfo()
	{
		Tag = FGameplayTag();
		Events = TArray<UEventHandleBase*>();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Event"))
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced)
	TArray<UEventHandleBase*> Events;
};

namespace GameplayTags
{
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_EventRoot);
};
