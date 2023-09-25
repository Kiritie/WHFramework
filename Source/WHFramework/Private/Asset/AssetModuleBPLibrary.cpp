// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModuleBPLibrary.h"

#include "Asset/AssetModule.h"
#include "Common/CommonBPLibrary.h"

UObject* UAssetModuleBPLibrary::FindObject(UClass* InClass, const FString& InName, bool bExactClass)
{
	if(!UCommonBPLibrary::IsPlaying())
	{
		return StaticFindObject(InClass, nullptr, *InName, bExactClass);
	}
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->FindObject(InClass, InName, bExactClass);
	}
	return nullptr;
}

UEnum* UAssetModuleBPLibrary::FindEnumByValue(const FString& InEnumName, int32 InEnumValue, bool bExactClass)
{
	if(!UCommonBPLibrary::IsPlaying())
	{
		return Cast<UEnum>(StaticFindObject(UEnum::StaticClass(), nullptr, *InEnumName, bExactClass));
	}
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->FindEnumByValue(InEnumName, InEnumValue, bExactClass);
	}
	return nullptr;
}

UEnum* UAssetModuleBPLibrary::FindEnumByValueName(const FString& InEnumName, const FString& InEnumValueName, bool bExactClass)
{
	if(!UCommonBPLibrary::IsPlaying())
	{
		return Cast<UEnum>(StaticFindObject(UEnum::StaticClass(), nullptr, *InEnumName, bExactClass));
	}
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->FindEnumByValueName(InEnumName, InEnumValueName, bExactClass);
	}
	return nullptr;
}

void UAssetModuleBPLibrary::AddEnumMapping(const FString& InEnumName, const FString& InOtherName)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		AssetModule->AddEnumMapping(InEnumName, InOtherName);
	}
}

void UAssetModuleBPLibrary::RemoveEnumMapping(const FString& InEnumName, const FString& InOtherName)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		AssetModule->RemoveEnumMapping(InEnumName, InOtherName);
	}
}

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
