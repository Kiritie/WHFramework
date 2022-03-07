// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssetModule.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Main/MainModule.h"
#include "AssetModuleBPLibrary.generated.h"

class UDataAssetBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAssetModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template<class T>
	static bool HasDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass = T::StaticClass())
	{
		if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
		{
			return AssetModule->HasDataAsset<T>(InDataAssetClass);
		}
		return false;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasDataAsset"))
	static bool K2_HasDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass);

	template<class T>
	static T* GetDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass = T::StaticClass())
	{
		if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
		{
			return AssetModule->GetDataAsset<T>(InDataAssetClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetDataAsset", DeterminesOutputType = "InDataAssetClass"))
	static UDataAssetBase* K2_GetDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass);

	template<class T>
	static T* GetDataAssetByName(FName InName)
	{
		if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
		{
			return AssetModule->GetDataAssetByName<T>(InName);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetDataAssetByName", DeterminesOutputType = "InDataAssetClass"))
	static UDataAssetBase* K2_GetDataAssetByName(FName InName, TSubclassOf<UDataAssetBase> InDataAssetClass);

	template<class T>
	static T* CreateDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass = T::StaticClass())
	{
		if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
		{
			return AssetModule->CreateDataAsset<T>(InDataAssetClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateDataAsset", DeterminesOutputType = "InDataAssetClass"))
	static UDataAssetBase* K2_CreateDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass);

	template<class T>
	static bool RemoveDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass = T::StaticClass())
	{
		if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
		{
			return AssetModule->RemoveDataAsset<T>(InDataAssetClass);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RemoveDataAsset"))
	static bool K2_RemoveDataAsset(TSubclassOf<UDataAssetBase>  InDataAssetClass);

	UFUNCTION(BlueprintCallable)
	static void RemoveAllDataAsset();
};
