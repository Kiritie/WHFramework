// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"
#include "Ability/AbilityModuleTypes.h"
#include "Data/DataAssetBase.h"

#include "Main/Base/ModuleBase.h"

#include "AssetModule.generated.h"

UCLASS()
class WHFRAMEWORK_API AAssetModule : public AModuleBase
{
	GENERATED_BODY()
		
	MODULE_INSTANCE_DECLARE(AAssetModule)

public:	
	// ParamSets default values for this actor's properties
	AAssetModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif
	
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// DataAsset
protected:
	UPROPERTY(EditAnywhere, Category = "DataAsset")
	TArray<UDataAssetBase*> DataAssets;

	UPROPERTY(VisibleAnywhere, Transient, Category = "DataAsset")
	TMap<FName, UDataAssetBase*> DataAssetMap;
	
public:
	UFUNCTION(BlueprintPure)
	bool HasDataAsset(FName InDataAssetName) const;

	template<class T>
	T* GetDataAsset(FName InDataAssetName = NAME_None) const
	{
		if(InDataAssetName.IsNone()) InDataAssetName = Cast<UDataAssetBase>(T::StaticClass()->GetDefaultObject())->GetDataAssetName();

		if(DataAssetMap.Contains(InDataAssetName))
		{
			return Cast<T>(DataAssetMap[InDataAssetName]);
		}
		return nullptr;
	}

	template<class T>
	T& GetDataAssetRef(FName InDataAssetName = NAME_None) const
	{
		if(T* Asset = GetDataAsset<T>(InDataAssetName))
		{
			return *Asset;
		}
		else
		{
			return UReferencePoolModuleBPLibrary::GetReference<T>();
		}
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InDataAssetClass"))
	UDataAssetBase* GetDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName = NAME_None) const;

	template<class T>
	T* CreateDataAsset(FName InDataAssetName = NAME_None)
	{
		if(InDataAssetName.IsNone()) InDataAssetName = Cast<UDataAssetBase>(T::StaticClass()->GetDefaultObject())->GetDataAssetName();

		if(UDataAssetBase* DataAsset = NewObject<UDataAssetBase>(this, T::StaticClass()))
		{
			if(!DataAssetMap.Contains(InDataAssetName))
			{
				DataAssetMap.Add(InDataAssetName, DataAsset);
			}
			return Cast<T>(DataAsset);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InDataAssetClass"))
	UDataAssetBase* CreateDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName = NAME_None);

	template<class T>
	bool RemoveDataAsset(FName InDataAssetName = NAME_None)
	{
		if(InDataAssetName.IsNone()) InDataAssetName = Cast<UDataAssetBase>(T::StaticClass()->GetDefaultObject())->GetDataAssetName();

		if(DataAssetMap.Contains(InDataAssetName))
		{
			if(UDataAssetBase* DataAsset = DataAssetMap[InDataAssetName])
			{
				DataAsset->ConditionalBeginDestroy();
				DataAssetMap.Remove(InDataAssetName);
			}
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable)
	bool RemoveDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName = NAME_None);

	UFUNCTION(BlueprintCallable)
	void RemoveAllDataAsset();

	//////////////////////////////////////////////////////////////////////////
	/// DataTable
protected:
	UPROPERTY(EditAnywhere, Category = "DataTable")
	TArray<UDataTable*> DataTables;

	UPROPERTY(VisibleAnywhere, Transient, Category = "DataTable")
	TMap<UScriptStruct*, UDataTable*> DataTableMap;

public:
	UFUNCTION(BlueprintCallable)
	bool AddDataTable(UDataTable* InDataTable);

	UFUNCTION(BlueprintCallable)
	bool RemoveDataTable(UDataTable* InDataTable);

	UFUNCTION(BlueprintCallable)
	void RemoveAllDataTable();

	template<class T>
	bool GetDataTableRow(int32 InRowIndex, T& OutRow)
	{
		UDataTable* DataTable = nullptr;
		FName RowName = NAME_None;
		if(DataTableMap.Contains(T::StaticStruct()))
		{
			DataTable = DataTableMap[T::StaticStruct()];
			if(DataTable)
			{
				TArray<FName> RowNames = DataTable->GetRowNames();
				if(RowNames.IsValidIndex(InRowIndex))
				{
					RowName = RowNames[InRowIndex];
				}
			}
		}
		return GetDataTableRow(DataTable, RowName, OutRow);
	}

	template<class T>
	bool GetDataTableRow(FName InRowName, T& OutRow)
	{
		UDataTable* DataTable = nullptr;
		if(DataTableMap.Contains(T::StaticStruct()))
		{
			DataTable = DataTableMap[T::StaticStruct()];
		}
		return GetDataTableRow(DataTable, InRowName, OutRow);
	}

	template<class T>
	bool GetDataTableRow(UDataTable* InDataTable, int32 InRowIndex, T& OutRow)
	{
		return GetDataTableRow(InDataTable, *FString::FromInt(InRowIndex), OutRow);
	}

	template<class T>
	bool GetDataTableRow(UDataTable* InDataTable, FName InRowName, T& OutRow)
	{
		if(!InDataTable) return false;

		FString ContextStr;
		if(T* Row = InDataTable->FindRow<T>(InRowName, ContextStr))
		{
			OutRow = *Row;
			return true;
		}
		return false;
	}

	template<class T>
	bool ReadDataTable(TArray<T>& OutRows)
	{
		UDataTable* DataTable = nullptr;
		if(DataTableMap.Contains(T::StaticStruct()))
		{
			DataTable = DataTableMap[T::StaticStruct()];
		}
		return ReadDataTable(DataTable, OutRows);
	}

	template<class T>
	bool ReadDataTable(UDataTable* InDataTable, TArray<T>& OutRows)
	{
		if(!InDataTable) return false;

		TArray<T*> Rows;
		FString ContextStr;

		InDataTable->GetAllRows(ContextStr, Rows);
		for(auto Iter : Rows)
		{
			OutRows.Add(*Iter);
		}
		return Rows.Num() > 0;
	}

	template<class T>
	bool ReadDataTable(TMap<FName, T>& OutRows)
	{
		UDataTable* DataTable = nullptr;
		if(DataTableMap.Contains(T::StaticStruct()))
		{
			DataTable = DataTableMap[T::StaticStruct()];
		}
		return ReadDataTable(DataTable, OutRows);
	}

	template<class T>
	bool ReadDataTable(UDataTable* InDataTable, TMap<FName, T>& OutRows)
	{
		if(!InDataTable) return false;

		FString ContextStr;

		for(auto Iter : InDataTable->GetRowNames())
		{
			if(T* Row = InDataTable->FindRow<T>(Iter, ContextStr))
			{
				OutRows.Add(Iter, *Row);
			}
		}
		return OutRows.Num() > 0;
	}
};
