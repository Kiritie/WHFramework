// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModuleBPLibrary.h"

#include "Asset/AssetModule.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

bool UAssetModuleBPLibrary::HasDataAsset(FName InDataAssetName)
{
	if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
	{
		return AssetModule->HasDataAsset(InDataAssetName);
	}
	return false;
}

UDataAssetBase* UAssetModuleBPLibrary::K2_GetDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName)
{
	if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
	{
		return AssetModule->K2_GetDataAsset(InDataAssetClass, InDataAssetName);
	}
	return nullptr;
}

UDataAssetBase* UAssetModuleBPLibrary::K2_CreateDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName)
{
	if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
	{
		return AssetModule->K2_CreateDataAsset(InDataAssetClass, InDataAssetName);
	}
	return nullptr;
}

bool UAssetModuleBPLibrary::K2_RemoveDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName)
{
	if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
	{
		return AssetModule->K2_RemoveDataAsset(InDataAssetClass, InDataAssetName);
	}
	return false;
}

void UAssetModuleBPLibrary::RemoveAllDataAsset()
{
	if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
	{
		AssetModule->RemoveAllDataAsset();
	}
}

UPrimaryAssetBase* UAssetModuleBPLibrary::LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning)
{
	return UPrimaryAssetManager::Get().LoadAsset(InPrimaryAssetId, bLogWarning);
}
