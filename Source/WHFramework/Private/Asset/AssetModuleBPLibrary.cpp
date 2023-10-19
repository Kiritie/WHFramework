// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModuleBPLibrary.h"

#include "Asset/AssetModule.h"

void UAssetModuleBPLibrary::AddStaticClass(const FName InName, const FStaticClass& InStaticClass)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		AssetModule->AddStaticClass(InName, InStaticClass);
	}
}

UClass* UAssetModuleBPLibrary::GetStaticClass(const FName InName)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->GetStaticClass(InName);
	}
	return nullptr;
}

UClass* UAssetModuleBPLibrary::FindClass(const FString& InName, bool bExactClass)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->FindClass(InName, bExactClass);
	}
	return nullptr;
}

UClass* UAssetModuleBPLibrary::LoadClass(UClass* InClass, const FString& InName)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->LoadClass(InClass, InName);
	}
	return nullptr;
}

void UAssetModuleBPLibrary::AddStaticObject(const FName InName, const FStaticObject& InStaticObject)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		AssetModule->AddStaticObject(InName, InStaticObject);
	}
}

UObject* UAssetModuleBPLibrary::GetStaticObject(UClass* InClass, const FName InName)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->GetStaticObject(InClass, InName);
	}
	return nullptr;
}

UObject* UAssetModuleBPLibrary::FindObject(UClass* InClass, const FString& InName, bool bExactClass)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->FindObject(InClass, InName, bExactClass);
	}
	return nullptr;
}

UObject* UAssetModuleBPLibrary::LoadObject(UClass* InClass, const FString& InName)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->LoadObject(InClass, InName);
	}
	return nullptr;
}

UEnum* UAssetModuleBPLibrary::FindEnumByValue(const FString& InEnumName, int32 InEnumValue, bool bExactClass)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->FindEnumByValue(InEnumName, InEnumValue, bExactClass);
	}
	return nullptr;
}

UEnum* UAssetModuleBPLibrary::FindEnumByAuthoredName(const FString& InEnumName, const FString& InEnumAuthoredName, bool bExactClass)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->FindEnumByAuthoredName(InEnumName, InEnumAuthoredName, bExactClass);
	}
	return nullptr;
}

UEnum* UAssetModuleBPLibrary::FindEnumByDisplayName(const FString& InEnumName, const FString& InEnumDisplayName, bool bExactClass)
{
	if(AAssetModule* AssetModule = AAssetModule::Get())
	{
		return AssetModule->FindEnumByDisplayName(InEnumName, InEnumDisplayName, bExactClass);
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
