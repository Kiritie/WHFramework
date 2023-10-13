// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AssetModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Asset/Primary/PrimaryAssetBase.h"
#include "Primary/PrimaryEntityInterface.h"

#include "AssetModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAssetModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	//////////////////////////////////////////////////////////////////////////
	/// Objects
public:
	template<class T> 
	static T* FindObject(const FString& InName, bool bExactClass = false)
	{
		return Cast<T>(FindObject(T::StaticClass(), InName, bExactClass));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "AssetModuleBPLibrary")
	static UObject* FindObject(UClass* InClass, const FString& InName, bool bExactClass = false);

	//////////////////////////////////////////////////////////////////////////
	/// Enums
public:
	UFUNCTION(BlueprintPure)
	static UEnum* FindEnumByValue(const FString& InEnumName, int32 InEnumValue, bool bExactClass = false);

	UFUNCTION(BlueprintPure)
	static UEnum* FindEnumByValueName(const FString& InEnumName, const FString& InEnumValueName, bool bExactClass = false);

	UFUNCTION(BlueprintCallable)
	static void AddEnumMapping(const FString& InEnumName, const FString& InOtherName);

	UFUNCTION(BlueprintCallable)
	static void RemoveEnumMapping(const FString& InEnumName, const FString& InOtherName);

	//////////////////////////////////////////////////////////////////////////
	/// DataAsset
public:
	UFUNCTION(BlueprintPure, Category = "AssetModuleBPLibrary")
	static bool HasDataAsset(FName InName);

	template<class T>
	static T* GetDataAsset(FName InName = NAME_None)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->GetDataAsset<T>(InName);
		}
		return nullptr;
	}
	
	template<class T>
	static T& GetDataAssetRef(FName InName = NAME_None)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->GetDataAssetRef<T>(InName);
		}
		else
		{
			return UReferencePoolModuleBPLibrary::GetReference<T>();
		}
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "AssetModuleBPLibrary")
	static UDataAssetBase* GetDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName = NAME_None);

	template<class T>
	static T* CreateDataAsset(FName InName = NAME_None)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->CreateDataAsset<T>(InName);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass"), Category = "AssetModuleBPLibrary")
	static UDataAssetBase* CreateDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName = NAME_None);

	template<class T>
	static bool RemoveDataAsset(FName InName = NAME_None)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->RemoveDataAsset<T>(InName);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "AssetModuleBPLibrary")
	static bool RemoveDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "AssetModuleBPLibrary")
	static void RemoveAllDataAsset();

	//////////////////////////////////////////////////////////////////////////
	/// DataTable
public:
	UFUNCTION(BlueprintCallable, Category = "AssetModuleBPLibrary")
	static bool AddDataTable(UDataTable* InDataTable);

	UFUNCTION(BlueprintCallable, Category = "AssetModuleBPLibrary")
	static bool RemoveDataTable(UDataTable* InDataTable);

	template<class T>
	static bool GetDataTableRow(int32 InRowIndex, T& OutRow)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->GetDataTableRow<T>(InRowIndex, OutRow);
		}
		return false;
	}

	template<class T>
	static bool GetDataTableRow(FName InRowName, T& OutRow)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->GetDataTableRow<T>(InRowName, OutRow);
		}
		return false;
	}

	template<class T>
	static bool GetDataTableRow(UDataTable* InDataTable, int32 InRowIndex, T& OutRow)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->GetDataTableRow<T>(InDataTable, InRowIndex, OutRow);
		}
		return false;
	}

	template<class T>
	static bool GetDataTableRow(UDataTable* InDataTable, FName InRowName, T& OutRow)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->GetDataTableRow<T>(InDataTable, InRowName, OutRow);
		}
		return false;
	}

	template<class T>
	static bool ReadDataTable(TArray<T>& OutRows)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->ReadDataTable<T>(OutRows);
		}
		return false;
	}

	template<class T>
	static bool ReadDataTable(UDataTable* InDataTable, TArray<T>& OutRows)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->ReadDataTable<T>(InDataTable, OutRows);
		}
		return false;
	}

	template<class T>
	static bool ReadDataTable(TMap<FName, T>& OutRows)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->ReadDataTable<T>(OutRows);
		}
		return false;
	}

	template<class T>
	static bool ReadDataTable(UDataTable* InDataTable, TMap<FName, T>& OutRows)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->ReadDataTable<T>(InDataTable, OutRows);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "AssetModuleBPLibrary")
	static void RemoveAllDataTable();

	//////////////////////////////////////////////////////////////////////////
	/// PrimaryAsset
public:
	UFUNCTION(BlueprintCallable, Category = "AssetModuleBPLibrary")
	static void RegisterPrimaryAssetType(FPrimaryAssetType InPrimaryAssetType)
	{
		UAssetManagerBase::Get().RegisterPrimaryAssetType(InPrimaryAssetType);
	}

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InParams", DeterminesOutputType = "InPrimaryAssetClass", DisplayName = "LoadPrimaryAsset"), Category = "AssetModuleBPLibrary")
	static UPrimaryAssetBase* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, TSubclassOf<UPrimaryAssetBase> InPrimaryAssetClass = nullptr, bool bLogWarning = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAsset(InPrimaryAssetId, bLogWarning);
	}
	
	template<class T>
	static T* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAsset<T>(InPrimaryAssetId, bLogWarning);
	}
				
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InPrimaryAssetClass", DisplayName = "LoadEntityAsset"), Category = "AssetModuleBPLibrary")
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

	UFUNCTION(BlueprintPure, Category = "AssetModuleBPLibrary")
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
