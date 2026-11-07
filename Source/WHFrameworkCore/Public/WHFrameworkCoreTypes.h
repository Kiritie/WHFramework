// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WHFrameworkCoreTypes.generated.h"

UENUM(BlueprintType)
enum class EObjectDespawnMode : uint8
{
	Recovery,
	Destroy
};

// 不加编译会报错
USTRUCT()
struct FMyStruct
{
	GENERATED_BODY()

};

//////////////////////////////////////////////////////////////////////////
// Delegates
DECLARE_DYNAMIC_DELEGATE( FSimpleDynamicDelegate );
DECLARE_DYNAMIC_MULTICAST_DELEGATE( FSimpleDynamicMulticastDelegate );

//////////////////////////////////////////////////////////////////////////
// Global Functions
template<typename T>
extern void RecursiveSetItems(const T* Value, TFunction<TSet<T*>(const T*)> Func)
{
	for(auto Iter : Func(Value))
	{
		RecursiveSetItems(Iter, Func);
	}
}
template<typename T>
extern void RecursiveSetItems(const T& Value, TFunction<TSet<T>(const T&)> Func)
{
	for(auto Iter : Func(Value))
	{
		RecursiveSetItems(Iter, Func);
	}
}

template<typename T>
extern void RecursiveArrayItems(const T* Value, TFunction<TArray<T*>(const T*)> Func)
{
	for(auto Iter : Func(Value))
	{
		RecursiveArrayItems(Iter, Func);
	}
}
template<typename T>
extern void RecursiveArrayItems(const T& Value, TFunction<TArray<T>(const T&)> Func)
{
	for(auto Iter : Func(Value))
	{
		RecursiveArrayItems(Iter, Func);
	}
}

template<typename TOut, typename TIn>
extern TArray<TOut> CastArrayItems(const TArray<TIn>& InArray)
{
	TArray<TOut> OutArray;
	for(auto Iter : InArray)
	{
		OutArray.Add(Cast<TOut>(Iter));
	}
	return OutArray;
}

template<typename TOut, typename TIn>
extern TArray<TSharedPtr<TOut>> CastSharedPtrArrayItems(const TArray<TSharedPtr<TIn>>& InArray)
{
	TArray<TSharedPtr<TOut>> OutArray;
	for(auto Iter : InArray)
	{
		OutArray.Add(StaticCastSharedPtr<TOut>(Iter));
	}
	return OutArray;
}
