// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "ReferencePoolModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ReferencePoolModuleStatics.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UReferencePoolModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template<class T>
	static bool HasReference(TSubclassOf<T> InType = T::StaticClass())
	{
		return UReferencePoolModule::Get().HasReference<T>(InType);
	}

	template<class T>
	static void CreateReference(UObject* InObject = nullptr, TSubclassOf<T> InType = T::StaticClass())
	{
		UReferencePoolModule::Get().CreateReference<T>(InObject, InType);
	}

	template<class T>
	static T& GetReference(bool bReset = false, TSubclassOf<T> InType = T::StaticClass())
	{
		return UReferencePoolModule::Get().GetReference<T>(bReset, InType);
	}

	template<class T>
	static void ResetReference(TSubclassOf<T> InType = T::StaticClass())
	{
		UReferencePoolModule::Get().ResetReference<T>(InType);
	}

	template<class T>
	static void ClearReference(TSubclassOf<T> InType = T::StaticClass())
	{
		UReferencePoolModule::Get().ClearReference<T>(InType);
	}

	static void ClearAllReference();
};
