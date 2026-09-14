// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ReferencePoolModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ReferencePoolModuleStatics.generated.h"

UCLASS()
class WHFRAMEWORK_API UReferencePoolModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////////
	/// Runtime Reference
	UFUNCTION(BlueprintPure, Category = "ReferencePoolModule")
	static bool HasReference(TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable, Category = "ReferencePoolModule", meta = (DeterminesOutputType = "InType"))
	static UObject* GetReference(TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable, Category = "ReferencePoolModule")
	static bool ResetReference(TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable, Category = "ReferencePoolModule")
	static bool ClearReference(TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable, Category = "ReferencePoolModule")
	static void ClearAllReference();

	template<class T>
	static bool HasReference(TSubclassOf<T> InType = T::StaticClass())
	{
		UReferencePoolModule* Module = UReferencePoolModule::GetPtr();
		return Module ? Module->HasReference<T>(InType) : false;
	}

	template<class T>
	static T& GetReference(TSubclassOf<T> InType = T::StaticClass())
	{
		UReferencePoolModule* Module = UReferencePoolModule::GetPtr();
		checkf(Module, TEXT("ReferencePoolModule is not available."));
		return Module->GetReference<T>(InType);
	}

	template<class T>
	static bool ResetReference(TSubclassOf<T> InType = T::StaticClass())
	{
		UReferencePoolModule* Module = UReferencePoolModule::GetPtr();
		return Module ? Module->ResetReference<T>(InType) : false;
	}

	template<class T>
	static bool ClearReference(TSubclassOf<T> InType = T::StaticClass())
	{
		UReferencePoolModule* Module = UReferencePoolModule::GetPtr();
		return Module ? Module->ClearReference<T>(InType) : false;
	}

	//////////////////////////////////////////////////////////////////////////
	/// Default Reference
	UFUNCTION(BlueprintPure, Category = "ReferencePoolModule", meta = (DeterminesOutputType = "InType"))
	static UObject* GetDefaultReference(TSubclassOf<UObject> InType);

	template<class T>
	static T& GetDefaultReference(TSubclassOf<T> InType = T::StaticClass())
	{
		checkf(InType, TEXT("Default reference class is null."));
		T* DefaultObject = CastChecked<T>(InType->GetDefaultObject());
		checkf(DefaultObject, TEXT("Failed to get default reference for class %s."), *GetNameSafe(InType.Get()));
		return *DefaultObject;
	}
};
