// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AssetModule.h"
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
	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics")
	static void AddStaticClass(const FName InName, const FStaticClass& InStaticClass);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "AssetModuleStatics")
	static UClass* GetStaticClass(const FName InName);

	UFUNCTION(BlueprintPure, Category = "AssetModuleStatics")
	static UClass* FindClass(const FString& InName, bool bExactClass = false);

	template<class T> 
	static UClass* LoadClass(const FString& InName)
	{
		return LoadClass(T::StaticClass(), InName);
	}

	UFUNCTION(BlueprintPure, Category = "AssetModuleStatics")
	static UClass* LoadClass(UClass* InClass, const FString& InName);

	//////////////////////////////////////////////////////////////////////////
	/// Objects
public:
	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics")
	static void AddStaticObject(const FName InName, const FStaticObject& InStaticObject);

	template<class T> 
	static T* GetStaticObject(const FName InName)
	{
		return Cast<T>(GetStaticObject(T::StaticClass(), InName));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "AssetModuleStatics")
	static UObject* GetStaticObject(UClass* InClass, const FName InName);

	template<class T> 
	static T* FindObject(const FString& InName, bool bExactClass = false)
	{
		return Cast<T>(FindObject(T::StaticClass(), InName, bExactClass));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "AssetModuleStatics")
	static UObject* FindObject(UClass* InClass, const FString& InName, bool bExactClass = false);

	template<class T> 
	static T* LoadObject(const FString& InName)
	{
		return Cast<T>(LoadObject(T::StaticClass(), InName));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "AssetModuleStatics")
	static UObject* LoadObject(UClass* InClass, const FString& InName);

	//////////////////////////////////////////////////////////////////////////
	/// Enums
public:
	UFUNCTION(BlueprintPure, Category = "AssetModuleStatics")
	static UEnum* FindEnumByValue(const FString& InEnumName, int32 InEnumValue, bool bExactClass = false);

	UFUNCTION(BlueprintPure, Category = "AssetModuleStatics")
	static UEnum* FindEnumByAuthoredName(const FString& InEnumName, const FString& InEnumAuthoredName, bool bExactClass = false);

	UFUNCTION(BlueprintPure, Category = "AssetModuleStatics")
	static UEnum* FindEnumByDisplayName(const FString& InEnumName, const FString& InEnumDisplayName, bool bExactClass = false);

	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics")
	static void AddEnumMapping(const FString& InEnumName, const FString& InOtherName);

	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics")
	static void RemoveEnumMapping(const FString& InEnumName, const FString& InOtherName);

	//////////////////////////////////////////////////////////////////////////
	/// DataAsset
public:
	UFUNCTION(BlueprintPure, Category = "AssetModuleStatics")
	static bool HasDataAsset(FName InName);

	template<class T>
	static T* GetDataAsset(FName InName = NAME_None)
	{
		return UAssetModule::Get().GetDataAsset<T>(InName);
	}
	
	template<class T>
	static T& GetDataAssetRef(FName InName = NAME_None)
	{
		return UAssetModule::Get().GetDataAssetRef<T>(InName);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "AssetModuleStatics")
	static UDataAssetBase* GetDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName = NAME_None);

	template<class T>
	static T* CreateDataAsset(FName InName = NAME_None)
	{
		return UAssetModule::Get().CreateDataAsset<T>(InName);
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"), Category = "AssetModuleStatics")
	static UDataAssetBase* CreateDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName = NAME_None);

	template<class T>
	static bool RemoveDataAsset(FName InName = NAME_None)
	{
		return UAssetModule::Get().RemoveDataAsset<T>(InName);
	}

	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics")
	static bool RemoveDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics")
	static void RemoveAllDataAsset();

	//////////////////////////////////////////////////////////////////////////
	/// DataTable
public:
	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics")
	static bool AddDataTable(UDataTable* InDataTable);

	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics")
	static bool RemoveDataTable(UDataTable* InDataTable);

	template<class T>
	static bool GetDataTableRow(int32 InRowIndex, T& OutRow)
	{
		return UAssetModule::Get().GetDataTableRow<T>(InRowIndex, OutRow);
	}

	template<class T>
	static bool GetDataTableRow(FName InRowName, T& OutRow)
	{
		return UAssetModule::Get().GetDataTableRow<T>(InRowName, OutRow);
	}

	template<class T>
	static bool GetDataTableRow(UDataTable* InDataTable, int32 InRowIndex, T& OutRow)
	{
		return UAssetModule::Get().GetDataTableRow<T>(InDataTable, InRowIndex, OutRow);
	}

	template<class T>
	static bool GetDataTableRow(UDataTable* InDataTable, FName InRowName, T& OutRow)
	{
		return UAssetModule::Get().GetDataTableRow<T>(InDataTable, InRowName, OutRow);
	}

	template<class T>
	static bool ReadDataTable(TArray<T>& OutRows)
	{
		return UAssetModule::Get().ReadDataTable<T>(OutRows);
	}

	template<class T>
	static bool ReadDataTable(UDataTable* InDataTable, TArray<T>& OutRows)
	{
		return UAssetModule::Get().ReadDataTable<T>(InDataTable, OutRows);
	}

	template<class T>
	static bool ReadDataTable(TMap<FName, T>& OutRows)
	{
		return UAssetModule::Get().ReadDataTable<T>(OutRows);
	}

	template<class T>
	static bool ReadDataTable(UDataTable* InDataTable, TMap<FName, T>& OutRows)
	{
		return UAssetModule::Get().ReadDataTable<T>(InDataTable, OutRows);
	}

	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics")
	static void RemoveAllDataTable();

	//////////////////////////////////////////////////////////////////////////
	/// PrimaryAsset
public:
	UFUNCTION(BlueprintCallable, Category = "AssetModuleStatics")
	static void RegisterPrimaryAssetType(FPrimaryAssetType InPrimaryAssetType)
	{
		UAssetManagerBase::Get().RegisterPrimaryAssetType(InPrimaryAssetType);
	}

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InParams", DeterminesOutputType = "InPrimaryAssetClass", DisplayName = "LoadPrimaryAsset"), Category = "AssetModuleStatics")
	static UPrimaryAssetBase* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, TSubclassOf<UPrimaryAssetBase> InPrimaryAssetClass = nullptr, bool bLogWarning = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAsset(InPrimaryAssetId, bLogWarning);
	}
	
	template<class T>
	static T* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAsset<T>(InPrimaryAssetId, bLogWarning);
	}
				
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InPrimaryAssetClass", DisplayName = "LoadEntityAsset"), Category = "AssetModuleStatics")
	static UPrimaryAssetBase* K2_LoadEntityAsset(const TScriptInterface<IPrimaryEntityInterface>& InPrimaryEntity, TSubclassOf<UPrimaryAssetBase> InPrimaryAssetClass = nullptr, bool bLogWarning = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAsset(InPrimaryEntity->Execute_GetAssetID(InPrimaryEntity.GetObject()), bLogWarning);
	}

	template<class T>
	static T* LoadEntityAsset(IPrimaryEntityInterface* InPrimaryEntity, bool bLogWarning = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAsset<T>(InPrimaryEntity->Execute_GetAssetID(Cast<UObject>(InPrimaryEntity)), bLogWarning);
	}

	template<class T>
	static T& LoadPrimaryAssetRef(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAssetRef<T>(InPrimaryAssetId, bLogWarning);
	}

	template<class T>
	static T& LoadEntityAssetRef(IPrimaryEntityInterface* InPrimaryEntity, bool bLogWarning = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAssetRef<T>(InPrimaryEntity->Execute_GetAssetID(Cast<UObject>(InPrimaryEntity)), bLogWarning);
	}

	UFUNCTION(BlueprintPure, Category = "AssetModuleStatics")
	static TArray<UPrimaryAssetBase*> LoadPrimaryAssets(FPrimaryAssetType InPrimaryAssetType, bool bLogWarning = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAssets(InPrimaryAssetType, bLogWarning);
	}

	template<class T>
	static TArray<T*> LoadPrimaryAssets(FPrimaryAssetType InPrimaryAssetType, bool bLogWarning = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAssets<T>(InPrimaryAssetType, bLogWarning);
	}
};
