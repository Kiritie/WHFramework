// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssetModule.h"
#include "AssetModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Main/MainModule.h"
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
	/// DataAsset
public:
	UFUNCTION(BlueprintPure, Category = "AssetModuleBPLibrary")
	static bool HasDataAsset(FName InDataAssetName);

	template<class T>
	static T* GetDataAsset(FName InDataAssetName = NAME_None)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->GetDataAsset<T>(InDataAssetName);
		}
		return nullptr;
	}
	
	template<class T>
	static T& GetDataAssetRef(FName InDataAssetName = NAME_None)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->GetDataAssetRef<T>(InDataAssetName);
		}
		else
		{
			return UReferencePoolModuleBPLibrary::GetReference<T>();
		}
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InDataAssetClass"), Category = "AssetModuleBPLibrary")
	static UDataAssetBase* GetDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName = NAME_None);

	template<class T>
	static T* CreateDataAsset(FName InDataAssetName = NAME_None)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->CreateDataAsset<T>(InDataAssetName);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InDataAssetClass"), Category = "AssetModuleBPLibrary")
	static UDataAssetBase* CreateDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName = NAME_None);

	template<class T>
	static bool RemoveDataAsset(FName InDataAssetName = NAME_None)
	{
		if(AAssetModule* AssetModule = AAssetModule::Get())
		{
			return AssetModule->RemoveDataAsset<T>(InDataAssetName);
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, Category = "AssetModuleBPLibrary")
	static bool RemoveDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName = NAME_None);

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
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InPrimaryAssetClass", DisplayName = "LoadPrimaryAsset"), Category = "AssetModuleBPLibrary")
	static UPrimaryAssetBase* K2_LoadPrimaryAsset(FPrimaryAssetId InPrimaryAssetId, TSubclassOf<UPrimaryAssetBase> InPrimaryAssetClass = nullptr, bool bLogWarning = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAsset(InPrimaryAssetId, bLogWarning);
	}

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
		return UAssetManagerBase::Get().LoadPrimaryAsset(InPrimaryEntity->GetAssetID(), bLogWarning);
	}

	template<class T>
	static T* LoadEntityAsset(IPrimaryEntityInterface* InPrimaryEntity, bool bLogWarning = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAsset<T>(InPrimaryEntity->GetAssetID(), bLogWarning);
	}

	template<class T>
	static T& LoadPrimaryAssetRef(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAssetRef<T>(InPrimaryAssetId, bLogWarning);
	}

	template<class T>
	static T& LoadEntityAssetRef(IPrimaryEntityInterface* InPrimaryEntity, bool bLogWarning = true)
	{
		return UAssetManagerBase::Get().LoadPrimaryAssetRef<T>(InPrimaryEntity->GetAssetID(), bLogWarning);
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
