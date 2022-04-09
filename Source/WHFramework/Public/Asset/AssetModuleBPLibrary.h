// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssetModule.h"
#include "AssetModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Main/MainModule.h"
#include "Asset/Primary/PrimaryAssetBase.h"

#include "AssetModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAssetModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	//////////////////////////////////////////////////////////////////////////
	/// DataAsset
public:
	UFUNCTION(BlueprintPure, Category = "AssetModuleBPLibrary")
	static bool HasDataAsset(FName InDataAssetName);

	template<class T>
	static T* GetDataAsset(FName InDataAssetName = NAME_None)
	{
		if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
		{
			return AssetModule->GetDataAsset<T>(InDataAssetName);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetDataAsset", DeterminesOutputType = "InDataAssetClass"), Category = "AssetModuleBPLibrary")
	static UDataAssetBase* K2_GetDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName = NAME_None);

	template<class T>
	static T* CreateDataAsset(FName InDataAssetName = NAME_None)
	{
		if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
		{
			return AssetModule->CreateDataAsset<T>(InDataAssetName);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateDataAsset", DeterminesOutputType = "InDataAssetClass"), Category = "AssetModuleBPLibrary")
	static UDataAssetBase* K2_CreateDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName = NAME_None);

	template<class T>
	static bool RemoveDataAsset(FName InDataAssetName = NAME_None)
	{
		if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
		{
			return AssetModule->RemoveDataAsset<T>(InDataAssetName);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RemoveDataAsset"), Category = "AssetModuleBPLibrary")
	static bool K2_RemoveDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "AssetModuleBPLibrary")
	static void RemoveAllDataAsset();

	//////////////////////////////////////////////////////////////////////////
	/// PrimaryAsset
public:
	template<class T>
	static T* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true)
	{
		return Cast<T>(LoadPrimaryAsset(InPrimaryAssetId, T::StaticClass(), bLogWarning));
	}
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InPrimaryAssetClass"), Category = "AssetModuleBPLibrary")
	static UPrimaryAssetBase* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, TSubclassOf<UPrimaryAssetBase> InPrimaryAssetClass = nullptr, bool bLogWarning = true);
};
