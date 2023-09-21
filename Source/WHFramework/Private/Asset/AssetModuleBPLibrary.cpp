// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModuleBPLibrary.h"

#include "Asset/AssetModule.h"

bool UAssetModuleBPLibrary::HasDataAsset(FName InName)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->HasDataAsset(InName);
	}
	return false;
}

UDataAssetBase* UAssetModuleBPLibrary::GetDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->GetDataAsset(InClass, InName);
	}
	return nullptr;
}

UDataAssetBase* UAssetModuleBPLibrary::CreateDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->CreateDataAsset(InClass, InName);
	}
	return nullptr;
}

bool UAssetModuleBPLibrary::RemoveDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->RemoveDataAsset(InClass, InName);
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
