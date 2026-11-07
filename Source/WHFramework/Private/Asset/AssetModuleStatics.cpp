// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModuleStatics.h"

#include "Asset/AssetModule.h"

void UAssetModuleStatics::AddStaticClass(const UObject* WorldContextObject, const FName InName, const FStaticClass& InStaticClass)
{
	UAssetModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).AddStaticClass(InName, InStaticClass);
}

UClass* UAssetModuleStatics::GetStaticClass(const UObject* WorldContextObject, const FName InName)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->GetStaticClass(InName) : nullptr;
}

UClass* UAssetModuleStatics::FindClass(const UObject* WorldContextObject, const FString& InName, bool bExactClass)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->FindClass(InName, bExactClass) : nullptr;
}

UClass* UAssetModuleStatics::LoadClass(const UObject* WorldContextObject, UClass* InClass, const FString& InName)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->LoadClass(InClass, InName) : nullptr;
}

void UAssetModuleStatics::AddStaticObject(const UObject* WorldContextObject, const FName InName, const FStaticObject& InStaticObject)
{
	UAssetModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).AddStaticObject(InName, InStaticObject);
}

UObject* UAssetModuleStatics::GetStaticObject(const UObject* WorldContextObject, UClass* InClass, const FName InName)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->GetStaticObject(InClass, InName) : nullptr;
}

UObject* UAssetModuleStatics::FindObject(const UObject* WorldContextObject, UClass* InClass, const FString& InName, bool bExactClass)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->FindObject(InClass, InName, bExactClass) : nullptr;
}

UObject* UAssetModuleStatics::LoadObject(const UObject* WorldContextObject, UClass* InClass, const FString& InName)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->LoadObject(InClass, InName) : nullptr;
}

FText UAssetModuleStatics::GetLocalizedText(const UObject* WorldContextObject, FName InNamespace, const FString& InKey)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->GetLocalizedText(InNamespace, InKey) : FText::GetEmpty();
}

FName UAssetModuleStatics::GetRandomTextKey(const UObject* WorldContextObject, FName InNamespace, const FString& InPrefix, int32 InSeed)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->GetRandomTextKey(InNamespace, InPrefix, InSeed) : NAME_None;
}

UEnum* UAssetModuleStatics::FindEnumByValue(const UObject* WorldContextObject, const FString& InEnumName, int32 InEnumValue, bool bExactClass)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->FindEnumByValue(InEnumName, InEnumValue, bExactClass) : nullptr;
}

UEnum* UAssetModuleStatics::FindEnumByAuthoredName(const UObject* WorldContextObject, const FString& InEnumName, const FString& InEnumAuthoredName, bool bExactClass)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->FindEnumByAuthoredName(InEnumName, InEnumAuthoredName, bExactClass) : nullptr;
}

UEnum* UAssetModuleStatics::FindEnumByDisplayName(const UObject* WorldContextObject, const FString& InEnumName, const FString& InEnumDisplayName, bool bExactClass)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->FindEnumByDisplayName(InEnumName, InEnumDisplayName, bExactClass) : nullptr;
}

void UAssetModuleStatics::AddEnumMapping(const UObject* WorldContextObject, const FString& InEnumName, const FString& InOtherName)
{
	UAssetModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).AddEnumMapping(InEnumName, InOtherName);
}

void UAssetModuleStatics::RemoveEnumMapping(const UObject* WorldContextObject, const FString& InEnumName, const FString& InOtherName)
{
	UAssetModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).RemoveEnumMapping(InEnumName, InOtherName);
}

bool UAssetModuleStatics::HasDataAsset(const UObject* WorldContextObject, const FGameplayTag& InTag)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->HasDataAsset(InTag) : false;
}

UDataAssetBase* UAssetModuleStatics::GetDataAsset(const UObject* WorldContextObject, TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->GetDataAsset(InClass, InTag) : nullptr;
}

UDataAssetBase* UAssetModuleStatics::CreateDataAsset(const UObject* WorldContextObject, TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->CreateDataAsset(InClass, InTag) : nullptr;
}

bool UAssetModuleStatics::RemoveDataAsset(const UObject* WorldContextObject, TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->RemoveDataAsset(InClass, InTag) : false;
}

void UAssetModuleStatics::RemoveAllDataAsset(const UObject* WorldContextObject)
{
	UAssetModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).RemoveAllDataAsset();
}

bool UAssetModuleStatics::AddDataTable(const UObject* WorldContextObject, UDataTable* InDataTable)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->AddDataTable(InDataTable) : false;
}

bool UAssetModuleStatics::RemoveDataTable(const UObject* WorldContextObject, UDataTable* InDataTable)
{
	UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Module ? Module->RemoveDataTable(InDataTable) : false;
}

void UAssetModuleStatics::RemoveAllDataTable(const UObject* WorldContextObject)
{
	UAssetModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).RemoveAllDataTable();
}
