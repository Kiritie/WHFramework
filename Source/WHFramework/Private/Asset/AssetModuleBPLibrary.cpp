// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModuleBPLibrary.h"

#include "Asset/AssetModule.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

bool UAssetModuleBPLibrary::HasDataAsset(FName InDataAssetName)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->HasDataAsset(InDataAssetName);
	}
	return false;
}

UDataAssetBase* UAssetModuleBPLibrary::GetDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->GetDataAsset(InDataAssetClass, InDataAssetName);
	}
	return nullptr;
}

UDataAssetBase* UAssetModuleBPLibrary::CreateDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->CreateDataAsset(InDataAssetClass, InDataAssetName);
	}
	return nullptr;
}

bool UAssetModuleBPLibrary::RemoveDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->RemoveDataAsset(InDataAssetClass, InDataAssetName);
	}
	return false;
}

void UAssetModuleBPLibrary::RemoveAllDataAsset()
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		AssetModule->RemoveAllDataAsset();
	}
}

bool UAssetModuleBPLibrary::AddDataTable(UDataTable* InDataTable)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->AddDataTable(InDataTable);
	}
	return false;
}

bool UAssetModuleBPLibrary::RemoveDataTable(UDataTable* InDataTable)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->RemoveDataTable(InDataTable);
	}
	return false;
}

void UAssetModuleBPLibrary::RemoveAllDataTable()
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		AssetModule->RemoveAllDataTable();
	}
}
