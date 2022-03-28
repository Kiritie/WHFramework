// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModuleBPLibrary.h"

#include "Asset/AssetModule.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

bool UAssetModuleBPLibrary::K2_HasDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass)
{
	if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
	{
		return AssetModule->K2_HasDataAsset(InDataAssetClass);
	}
	return false;
}

UDataAssetBase* UAssetModuleBPLibrary::K2_GetDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass)
{
	if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
	{
		return AssetModule->K2_GetDataAsset(InDataAssetClass);
	}
	return nullptr;
}

UDataAssetBase* UAssetModuleBPLibrary::K2_GetDataAssetByName(FName InName, TSubclassOf<UDataAssetBase> InDataAssetClass)
{
	if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
	{
		return AssetModule->K2_GetDataAssetByName(InName, InDataAssetClass);
	}
	return nullptr;
}

UDataAssetBase* UAssetModuleBPLibrary::K2_CreateDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass)
{
	if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
	{
		return AssetModule->K2_CreateDataAsset(InDataAssetClass);
	}
	return nullptr;
}

bool UAssetModuleBPLibrary::K2_RemoveDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass)
{
	if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
	{
		return AssetModule->K2_RemoveDataAsset(InDataAssetClass);
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

FPrimaryAssetType UAssetModuleBPLibrary::GetPrimaryAssetTypeByItemType(EItemType InItemType)
{
	return *UGlobalBPLibrary::GetEnumValueAuthoredName(TEXT("EItemType"), (int32)InItemType);
}

UPrimaryAssetBase* UAssetModuleBPLibrary::LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning)
{
	return UPrimaryAssetManager::Get().LoadAsset(InPrimaryAssetId, bLogWarning);
}
