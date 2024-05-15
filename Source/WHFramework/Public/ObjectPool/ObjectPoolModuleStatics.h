// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "ObjectPoolModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ObjectPoolModuleStatics.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UObjectPoolModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template<class T>
	static bool HasObject(bool bInEditor = false, TSubclassOf<UObject> InType = T::StaticClass())
	{
		return HasObject(InType, bInEditor);
	}

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModuleStatics")
	static bool HasObject(TSubclassOf<UObject> InType, bool bInEditor = false);
	
	template<class T>
	static T* SpawnObject(UObject* InOwner = nullptr, const TArray<FParameter>* InParams = nullptr, bool bInEditor = false, TSubclassOf<UObject> InType = T::StaticClass())
	{
		return UObjectPoolModule::Get(bInEditor).SpawnObject<T>(InOwner, InParams, InType);
	}
	
	template<class T>
	static T* SpawnObject(UObject* InOwner, const TArray<FParameter>& InParams, bool bInEditor = false, TSubclassOf<UObject> InType = T::StaticClass())
	{
		return UObjectPoolModule::Get(bInEditor).SpawnObject<T>(InOwner, InParams, InType);
	}
	
	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InType", AutoCreateRefTerm = "InParams"), Category = "ObjectPoolModuleStatics")
	static UObject* SpawnObject(TSubclassOf<UObject> InType, UObject* InOwner, const TArray<FParameter>& InParams, bool bInEditor = false);

	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModuleStatics")
	static void DespawnObject(UObject* InObject, bool bRecovery = true, bool bInEditor = false);
	
	template<class T>
	static void DespawnObjects(TArray<T*> InObjects, bool bRecovery = true, bool bInEditor = false)
	{
		for(auto Iter : InObjects)
		{
			DespawnObject(Iter, bRecovery, bInEditor);
		}
	}
	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModuleStatics")
	static void DespawnObjects(TArray<UObject*> InObjects, bool bRecovery = true, bool bInEditor = false);

	template<class T>
	static void ClearObject(bool bInEditor = false, TSubclassOf<UObject> InType = T::StaticClass())
	{
		UObjectPoolModule::Get().ClearObject<T>(InType, bInEditor);
	}
	
	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModuleStatics")
	static void ClearObject(TSubclassOf<UObject> InType, bool bInEditor = false);
	
	UFUNCTION(BlueprintCallable, Category = "ObjectPoolModuleStatics")
	static void ClearAllObject(bool bInEditor = false);
};
