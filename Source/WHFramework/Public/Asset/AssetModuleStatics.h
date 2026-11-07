// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AssetManagerBase.h"
#include "Common/CommonTypes.h"
#include "AssetModule.h"
#include "Asset/AssetMappingTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "Primary/PrimaryEntityInterface.h"

#include "AssetModuleStatics.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAssetModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	//////////////////////////////////////////////////////////////////////////
	/// Classes
public:
	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void AddStaticClass(const UObject* WorldContextObject, const FName InName, const FStaticClass& InStaticClass);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = "AssetModuleStatics")
	static UClass* GetStaticClass(const UObject* WorldContextObject, const FName InName);

	UFUNCTION(BlueprintPure, Category = "AssetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static UClass* FindClass(const UObject* WorldContextObject, const FString& InName, bool bExactClass = false);

	template<class T> 
	static UClass* LoadClass(const UObject* WorldContextObject, const FString& InName)
	{
		return LoadClass(WorldContextObject, T::StaticClass(), InName);
	}

	UFUNCTION(BlueprintPure, Category = "AssetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static UClass* LoadClass(const UObject* WorldContextObject, UClass* InClass, const FString& InName);

	//////////////////////////////////////////////////////////////////////////
	/// Objects
public:
	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void AddStaticObject(const UObject* WorldContextObject, const FName InName, const FStaticObject& InStaticObject);

	template<class T> 
	static T* GetStaticObject(const UObject* WorldContextObject, const FName InName)
	{
		return Cast<T>(GetStaticObject(WorldContextObject, T::StaticClass(), InName));
	}

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "AssetModuleStatics")
	static UObject* GetStaticObject(const UObject* WorldContextObject, UClass* InClass, const FName InName);

	template<class T> 
	static T* FindObject(const UObject* WorldContextObject, const FString& InName, bool bExactClass = false)
	{
		return Cast<T>(FindObject(WorldContextObject, T::StaticClass(), InName, bExactClass));
	}

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "AssetModuleStatics")
	static UObject* FindObject(const UObject* WorldContextObject, UClass* InClass, const FString& InName, bool bExactClass = false);

	template<class T> 
	static T* LoadObject(const UObject* WorldContextObject, const FString& InName)
	{
		return Cast<T>(LoadObject(WorldContextObject, T::StaticClass(), InName));
	}

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "AssetModuleStatics")
	static UObject* LoadObject(const UObject* WorldContextObject, UClass* InClass, const FString& InName);

	//////////////////////////////////////////////////////////////////////////
	/// 本地化文本
public:
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static FText GetLocalizedText(const UObject* WorldContextObject, FName InNamespace, const FString& InKey);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static FName GetRandomTextKey(const UObject* WorldContextObject, FName InNamespace, const FString& InPrefix, int32 InSeed);

	//////////////////////////////////////////////////////////////////////////
	/// Enums
public:
	UFUNCTION(BlueprintPure, Category = "AssetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static UEnum* FindEnumByValue(const UObject* WorldContextObject, const FString& InEnumName, int32 InEnumValue, bool bExactClass = false);

	UFUNCTION(BlueprintPure, Category = "AssetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static UEnum* FindEnumByAuthoredName(const UObject* WorldContextObject, const FString& InEnumName, const FString& InEnumAuthoredName, bool bExactClass = false);

	UFUNCTION(BlueprintPure, Category = "AssetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static UEnum* FindEnumByDisplayName(const UObject* WorldContextObject, const FString& InEnumName, const FString& InEnumDisplayName, bool bExactClass = false);

	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void AddEnumMapping(const UObject* WorldContextObject, const FString& InEnumName, const FString& InOtherName);

	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void RemoveEnumMapping(const UObject* WorldContextObject, const FString& InEnumName, const FString& InOtherName);

	//////////////////////////////////////////////////////////////////////////
	/// DataAsset
public:
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "InTag"), Category = "AssetModuleStatics")
	static bool HasDataAsset(const UObject* WorldContextObject, const FGameplayTag& InTag);

	template<class T>
	static T* GetDataAsset(const UObject* WorldContextObject, const FGameplayTag& InTag = FGameplayTag())
	{
		UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		return Module ? Module->GetDataAsset<T>(InTag) : nullptr;
	}
	
	template<class T>
	static T& GetDataAssetRef(const UObject* WorldContextObject, const FGameplayTag& InTag = FGameplayTag())
	{
		return UAssetModule::Get(WorldContextObject ? WorldContextObject->GetWorld() : nullptr).GetDataAssetRef<T>(InTag);
	}

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass", AutoCreateRefTerm = "InTag"), Category = "AssetModuleStatics")
	static UDataAssetBase* GetDataAsset(const UObject* WorldContextObject, TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag = FGameplayTag());

	template<class T>
	static T* CreateDataAsset(const UObject* WorldContextObject, const FGameplayTag& InTag = FGameplayTag())
	{
		UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		return Module ? Module->CreateDataAsset<T>(InTag) : nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass", AutoCreateRefTerm = "InTag"), Category = "AssetModuleStatics")
	static UDataAssetBase* CreateDataAsset(const UObject* WorldContextObject, TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag = FGameplayTag());

	template<class T>
	static bool RemoveDataAsset(const UObject* WorldContextObject, const FGameplayTag& InTag = FGameplayTag())
	{
		UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		return Module ? Module->RemoveDataAsset<T>(InTag) : false;
	}

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "InTag"), Category = "AssetModuleStatics")
	static bool RemoveDataAsset(const UObject* WorldContextObject, TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag = FGameplayTag());

	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void RemoveAllDataAsset(const UObject* WorldContextObject);

	//////////////////////////////////////////////////////////////////////////
	/// DataTable
public:
	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool AddDataTable(const UObject* WorldContextObject, UDataTable* InDataTable);

	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static bool RemoveDataTable(const UObject* WorldContextObject, UDataTable* InDataTable);

	template<class T>
	static bool GetDataTableRow(const UObject* WorldContextObject, int32 InRowIndex, T& OutRow)
	{
		UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		return Module ? Module->GetDataTableRow<T>(InRowIndex, OutRow) : false;
	}

	template<class T>
	static bool GetDataTableRow(const UObject* WorldContextObject, FName InRowName, T& OutRow)
	{
		UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		return Module ? Module->GetDataTableRow<T>(InRowName, OutRow) : false;
	}

	template<class T>
	static bool GetDataTableRow(const UObject* WorldContextObject, UDataTable* InDataTable, int32 InRowIndex, T& OutRow)
	{
		UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		return Module ? Module->GetDataTableRow<T>(InDataTable, InRowIndex, OutRow) : false;
	}

	template<class T>
	static bool GetDataTableRow(const UObject* WorldContextObject, UDataTable* InDataTable, FName InRowName, T& OutRow)
	{
		UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		return Module ? Module->GetDataTableRow<T>(InDataTable, InRowName, OutRow) : false;
	}

	template<class T>
	static bool ReadDataTable(const UObject* WorldContextObject, TArray<T>& OutRows)
	{
		UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		return Module ? Module->ReadDataTable<T>(OutRows) : false;
	}

	template<class T>
	static bool ReadDataTable(const UObject* WorldContextObject, UDataTable* InDataTable, TArray<T>& OutRows)
	{
		UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		return Module ? Module->ReadDataTable<T>(InDataTable, OutRows) : false;
	}

	template<class T>
	static bool ReadDataTable(const UObject* WorldContextObject, TMap<FName, T>& OutRows)
	{
		UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		return Module ? Module->ReadDataTable<T>(OutRows) : false;
	}

	template<class T>
	static bool ReadDataTable(const UObject* WorldContextObject, UDataTable* InDataTable, TMap<FName, T>& OutRows)
	{
		UAssetModule* Module = UAssetModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
		return Module ? Module->ReadDataTable<T>(InDataTable, OutRows) : false;
	}

	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics", meta = (WorldContext = "WorldContextObject"))
	static void RemoveAllDataTable(const UObject* WorldContextObject);

	//////////////////////////////////////////////////////////////////////////
	/// PrimaryAsset
public:
	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InParams", DeterminesOutputType = "InPrimaryAssetClass", DisplayName = "LoadPrimaryAsset"), Category = "AssetModuleStatics")
	static UPrimaryAssetBase* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, TSubclassOf<UPrimaryAssetBase> InPrimaryAssetClass = nullptr, bool bEnsured = true)
	{
		return GetDeterminesOutputObject(UAssetManagerBase::Get().LoadPrimaryAsset(InPrimaryAssetId, bEnsured), InPrimaryAssetClass);
	}
	
	template<class T>
	static T* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bEnsured = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAsset<T>(InPrimaryAssetId, bEnsured);
	}
				
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InPrimaryAssetClass", DisplayName = "LoadEntityAsset"), Category = "AssetModuleStatics")
	static UPrimaryAssetBase* K2_LoadEntityAsset(const TScriptInterface<IPrimaryEntityInterface>& InPrimaryEntity, TSubclassOf<UPrimaryAssetBase> InPrimaryAssetClass = nullptr, bool bEnsured = true)
	{
		return GetDeterminesOutputObject(UAssetManagerBase::Get().LoadPrimaryAsset(InPrimaryEntity->Execute_GetAssetID(InPrimaryEntity.GetObject()), bEnsured), InPrimaryAssetClass);
	}

	template<class T>
	static T* LoadEntityAsset(IPrimaryEntityInterface* InPrimaryEntity, bool bEnsured = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAsset<T>(InPrimaryEntity->Execute_GetAssetID(Cast<UObject>(InPrimaryEntity)), bEnsured);
	}

	template<class T>
	static T& LoadPrimaryAssetRef(const FPrimaryAssetId& InPrimaryAssetId, bool bEnsured = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAssetRef<T>(InPrimaryAssetId, bEnsured);
	}

	template<class T>
	static T& LoadEntityAssetRef(IPrimaryEntityInterface* InPrimaryEntity, bool bEnsured = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAssetRef<T>(InPrimaryEntity->Execute_GetAssetID(Cast<UObject>(InPrimaryEntity)), bEnsured);
	}

	UFUNCTION(BlueprintPure, Category = "AssetModuleStatics")
	static TArray<UPrimaryAssetBase*> LoadPrimaryAssets(FPrimaryAssetType InPrimaryAssetType, bool bEnsured = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAssets(InPrimaryAssetType, bEnsured);
	}

	template<class T>
	static TArray<T*> LoadPrimaryAssets(FPrimaryAssetType InPrimaryAssetType, bool bEnsured = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAssets<T>(InPrimaryAssetType, bEnsured);
	}
};
