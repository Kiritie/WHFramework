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
		if(UReferencePoolModule* ReferencePoolModule = UReferencePoolModule::Get())
		{
			return ReferencePoolModule->HasReference<T>(InType);
		}
		return false;
	}

	template<class T>
	static void CreateReference(UObject* InObject = nullptr, TSubclassOf<T> InType = T::StaticClass())
	{
		if(UReferencePoolModule* ReferencePoolModule = UReferencePoolModule::Get())
		{
			ReferencePoolModule->CreateReference<T>(InObject, InType);
		}
	}

	template<class T>
	static T& GetReference(bool bReset = false, TSubclassOf<T> InType = T::StaticClass())
	{
		if(UReferencePoolModule* ReferencePoolModule = UReferencePoolModule::Get())
		{
			return ReferencePoolModule->GetReference<T>(bReset, InType);
		}
		return *NewObject<T>(GetTransientPackage(), InType);
	}

	template<class T>
	static void ResetReference(TSubclassOf<T> InType = T::StaticClass())
	{
		if(UReferencePoolModule* ReferencePoolModule = UReferencePoolModule::Get())
		{
			ReferencePoolModule->ResetReference<T>(InType);
		}
	}

	template<class T>
	static void ClearReference(TSubclassOf<T> InType = T::StaticClass())
	{
		if(UReferencePoolModule* ReferencePoolModule = UReferencePoolModule::Get())
		{
			ReferencePoolModule->ClearReference<T>(InType);
		}
	}

	static void ClearAllReference();
};
