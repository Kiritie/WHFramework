// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModuleStatics.h"

#include "Asset/AssetModule.h"

void UAssetModuleStatics::AddStaticClass(const FName InName, const FStaticClass& InStaticClass)
{
	UAssetModule::Get().AddStaticClass(InName, InStaticClass);
}

UClass* UAssetModuleStatics::GetStaticClass(const FName InName)
{
	return UAssetModule::Get().GetStaticClass(InName);
}

UClass* UAssetModuleStatics::FindClass(const FString& InName, bool bExactClass)
{
	return UAssetModule::Get().FindClass(InName, bExactClass);
}

UClass* UAssetModuleStatics::LoadClass(UClass* InClass, const FString& InName)
{
	return UAssetModule::Get().LoadClass(InClass, InName);
}

void UAssetModuleStatics::AddStaticObject(const FName InName, const FStaticObject& InStaticObject)
{
	UAssetModule::Get().AddStaticObject(InName, InStaticObject);
}

UObject* UAssetModuleStatics::GetStaticObject(UClass* InClass, const FName InName)
{
	return UAssetModule::Get().GetStaticObject(InClass, InName);
}

UObject* UAssetModuleStatics::FindObject(UClass* InClass, const FString& InName, bool bExactClass)
{
	return UAssetModule::Get().FindObject(InClass, InName, bExactClass);
}

UObject* UAssetModuleStatics::LoadObject(UClass* InClass, const FString& InName)
{
	return UAssetModule::Get().LoadObject(InClass, InName);
}

UEnum* UAssetModuleStatics::FindEnumByValue(const FString& InEnumName, int32 InEnumValue, bool bExactClass)
{
	return UAssetModule::Get().FindEnumByValue(InEnumName, InEnumValue, bExactClass);
}

UEnum* UAssetModuleStatics::FindEnumByAuthoredName(const FString& InEnumName, const FString& InEnumAuthoredName, bool bExactClass)
{
	return UAssetModule::Get().FindEnumByAuthoredName(InEnumName, InEnumAuthoredName, bExactClass);
}

UEnum* UAssetModuleStatics::FindEnumByDisplayName(const FString& InEnumName, const FString& InEnumDisplayName, bool bExactClass)
{
	return UAssetModule::Get().FindEnumByDisplayName(InEnumName, InEnumDisplayName, bExactClass);
}

void UAssetModuleStatics::AddEnumMapping(const FString& InEnumName, const FString& InOtherName)
{
	return UAssetModule::Get().AddEnumMapping(InEnumName, InOtherName);
}

void UAssetModuleStatics::RemoveEnumMapping(const FString& InEnumName, const FString& InOtherName)
{
	return UAssetModule::Get().RemoveEnumMapping(InEnumName, InOtherName);
}

bool UAssetModuleStatics::HasDataAsset(const FGameplayTag& InTag)
{
	return UAssetModule::Get().HasDataAsset(InTag);
}

UDataAssetBase* UAssetModuleStatics::GetDataAsset(TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag)
{
	return UAssetModule::Get().GetDataAsset(InClass, InTag);
}

UDataAssetBase* UAssetModuleStatics::CreateDataAsset(TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag)
{
	return UAssetModule::Get().CreateDataAsset(InClass, InTag);
}

bool UAssetModuleStatics::RemoveDataAsset(TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag)
{
	return UAssetModule::Get().RemoveDataAsset(InClass, InTag);
}

void UAssetModuleStatics::RemoveAllDataAsset()
{
	return UAssetModule::Get().RemoveAllDataAsset();
}

bool UAssetModuleStatics::AddDataTable(UDataTable* InDataTable)
{
	return UAssetModule::Get().AddDataTable(InDataTable);
}

bool UAssetModuleStatics::RemoveDataTable(UDataTable* InDataTable)
{
	return UAssetModule::Get().RemoveDataTable(InDataTable);
}

void UAssetModuleStatics::RemoveAllDataTable()
{
	return UAssetModule::Get().RemoveAllDataTable();
}
