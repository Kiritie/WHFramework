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

bool UAssetModuleBPLibrary::AddDataTable(UDataTable* InDataTable)
{
	if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
	{
		return AssetModule->AddDataTable(InDataTable);
	}
	return false;
}

bool UAssetModuleBPLibrary::RemoveDataTable(UDataTable* InDataTable)
{
	if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
	{
		return AssetModule->RemoveDataTable(InDataTable);
	}
	return false;
}

void UAssetModuleBPLibrary::RemoveAllDataTable()
{
	if(AAssetModule* AssetModule = AMainModule::GetModuleByClass<AAssetModule>())
	{
		AssetModule->RemoveAllDataTable();
	}
}

UPrimaryAssetBase* UAssetModuleBPLibrary::LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, TSubclassOf<UPrimaryAssetBase> InPrimaryAssetClass, bool bLogWarning)
{
	return UAssetManagerBase::Get().LoadPrimaryAsset(InPrimaryAssetId, bLogWarning);
}

TArray<UPrimaryAssetBase*> UAssetModuleBPLibrary::LoadPrimaryAssets(FPrimaryAssetType InPrimaryAssetType, bool bLogWarning)
{
	return UAssetManagerBase::Get().LoadPrimaryAssets(InPrimaryAssetType, bLogWarning);
}
