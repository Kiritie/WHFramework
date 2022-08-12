// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Main/MainModule.h"
#include "ReferencePoolModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ReferencePoolModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UReferencePoolModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template<class T>
	static bool HasReference(TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(AReferencePoolModule* ReferencePoolModule = AMainModule::GetModuleByClass<AReferencePoolModule>())
		{
			return ReferencePoolModule->HasReference<T>(InType);
		}
		return false;
	}

	template<class T>
	static void CreateReference(UObject* InObject = nullptr, TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(AReferencePoolModule* ReferencePoolModule = AMainModule::GetModuleByClass<AReferencePoolModule>())
		{
			ReferencePoolModule->CreateReference<T>(InObject, InType);
		}
	}

	template<class T>
	static T& GetReference(bool bReset = false, TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(AReferencePoolModule* ReferencePoolModule = AMainModule::GetModuleByClass<AReferencePoolModule>())
		{
			return ReferencePoolModule->GetReference<T>(bReset, InType);
		}
		return *NewObject<T>(GetTransientPackage(), InType);
	}

	template<class T>
	static void ResetReference(TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(AReferencePoolModule* ReferencePoolModule = AMainModule::GetModuleByClass<AReferencePoolModule>())
		{
			ReferencePoolModule->ResetReference<T>(InType);
		}
	}

	template<class T>
	static void ClearReference(TSubclassOf<UObject> InType = T::StaticClass())
	{
		if(AReferencePoolModule* ReferencePoolModule = AMainModule::GetModuleByClass<AReferencePoolModule>())
		{
			ReferencePoolModule->ClearReference<T>(InType);
		}
	}

	static void ClearAllReference();
};
