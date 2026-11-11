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
	UFUNCTION(BlueprintPure, Category = "ReferencePoolModule", meta = (WorldContext = "WorldContextObject"))
	static bool HasReference(const UObject* WorldContextObject, TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable, Category = "ReferencePoolModule", meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InType"))
	static UObject* GetReference(const UObject* WorldContextObject, TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable, Category = "ReferencePoolModule", meta = (WorldContext = "WorldContextObject"))
	static bool ResetReference(const UObject* WorldContextObject, TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable, Category = "ReferencePoolModule", meta = (WorldContext = "WorldContextObject"))
	static bool ClearReference(const UObject* WorldContextObject, TSubclassOf<UObject> InType);

	UFUNCTION(BlueprintCallable, Category = "ReferencePoolModule", meta = (WorldContext = "WorldContextObject"))
	static void ClearAllReference(const UObject* WorldContextObject);

	template<class T>
	static bool HasReference(const UObject* WorldContextObject, TSubclassOf<T> InType = T::StaticClass())
	{
		UReferencePoolModule* Module = UReferencePoolModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		return Module ? Module->HasReference<T>(InType) : false;
	}

	template<class T>
	static T& GetReference(const UObject* WorldContextObject, TSubclassOf<T> InType = T::StaticClass())
	{
		UReferencePoolModule* Module = UReferencePoolModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		checkf(Module, TEXT("ReferencePoolModule is not available."));
		return Module->GetReference<T>(InType);
	}

	template<class T>
	static bool ResetReference(const UObject* WorldContextObject, TSubclassOf<T> InType = T::StaticClass())
	{
		UReferencePoolModule* Module = UReferencePoolModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		return Module ? Module->ResetReference<T>(InType) : false;
	}

	template<class T>
	static bool ClearReference(const UObject* WorldContextObject, TSubclassOf<T> InType = T::StaticClass())
	{
		UReferencePoolModule* Module = UReferencePoolModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
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
