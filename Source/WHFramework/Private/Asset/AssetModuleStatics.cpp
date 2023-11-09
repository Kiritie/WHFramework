// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModuleStatics.h"

#include "Asset/AssetModule.h"

void UAssetModuleStatics::AddStaticClass(const FName InName, const FStaticClass& InStaticClass)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		AssetModule->AddStaticClass(InName, InStaticClass);
	}
}

UClass* UAssetModuleStatics::GetStaticClass(const FName InName)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->GetStaticClass(InName);
	}
	return nullptr;
}

UClass* UAssetModuleStatics::FindClass(const FString& InName, bool bExactClass)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->FindClass(InName, bExactClass);
	}
	return nullptr;
}

UClass* UAssetModuleStatics::LoadClass(UClass* InClass, const FString& InName)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->LoadClass(InClass, InName);
	}
	return nullptr;
}

void UAssetModuleStatics::AddStaticObject(const FName InName, const FStaticObject& InStaticObject)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		AssetModule->AddStaticObject(InName, InStaticObject);
	}
}

UObject* UAssetModuleStatics::GetStaticObject(UClass* InClass, const FName InName)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->GetStaticObject(InClass, InName);
	}
	return nullptr;
}

UObject* UAssetModuleStatics::FindObject(UClass* InClass, const FString& InName, bool bExactClass)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->FindObject(InClass, InName, bExactClass);
	}
	return nullptr;
}

UObject* UAssetModuleStatics::LoadObject(UClass* InClass, const FString& InName)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->LoadObject(InClass, InName);
	}
	return nullptr;
}

UEnum* UAssetModuleStatics::FindEnumByValue(const FString& InEnumName, int32 InEnumValue, bool bExactClass)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->FindEnumByValue(InEnumName, InEnumValue, bExactClass);
	}
	return nullptr;
}

UEnum* UAssetModuleStatics::FindEnumByAuthoredName(const FString& InEnumName, const FString& InEnumAuthoredName, bool bExactClass)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->FindEnumByAuthoredName(InEnumName, InEnumAuthoredName, bExactClass);
	}
	return nullptr;
}

UEnum* UAssetModuleStatics::FindEnumByDisplayName(const FString& InEnumName, const FString& InEnumDisplayName, bool bExactClass)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->FindEnumByDisplayName(InEnumName, InEnumDisplayName, bExactClass);
	}
	return nullptr;
}

void UAssetModuleStatics::AddEnumMapping(const FString& InEnumName, const FString& InOtherName)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		AssetModule->AddEnumMapping(InEnumName, InOtherName);
	}
}

void UAssetModuleStatics::RemoveEnumMapping(const FString& InEnumName, const FString& InOtherName)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		AssetModule->RemoveEnumMapping(InEnumName, InOtherName);
	}
}

bool UAssetModuleStatics::HasDataAsset(FName InName)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->HasDataAsset(InName);
	}
	return false;
}

UDataAssetBase* UAssetModuleStatics::GetDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->GetDataAsset(InClass, InName);
	}
	return nullptr;
}

UDataAssetBase* UAssetModuleStatics::CreateDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->CreateDataAsset(InClass, InName);
	}
	return nullptr;
}

bool UAssetModuleStatics::RemoveDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->RemoveDataAsset(InClass, InName);
	}
	return false;
}

void UAssetModuleStatics::RemoveAllDataAsset()
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		AssetModule->RemoveAllDataAsset();
	}
}

bool UAssetModuleStatics::AddDataTable(UDataTable* InDataTable)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->AddDataTable(InDataTable);
	}
	return false;
}

bool UAssetModuleStatics::RemoveDataTable(UDataTable* InDataTable)
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		return AssetModule->RemoveDataTable(InDataTable);
	}
	return false;
}

void UAssetModuleStatics::RemoveAllDataTable()
{
	if(UAssetModule* AssetModule = UAssetModule::Get())
	{
		AssetModule->RemoveAllDataTable();
	}
}
