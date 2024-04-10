// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Data/DataAssetBase.h"
#include "Main/Base/ModuleBase.h"
#include "ReferencePool/ReferencePoolModuleStatics.h"

#include "AssetModule.generated.h"

UCLASS()
class WHFRAMEWORK_API UAssetModule : public UModuleBase
{
	GENERATED_BODY()
		
	GENERATED_MODULE(UAssetModule)

public:	
	// ParamSets default values for this actor's properties
	UAssetModule();

	~UAssetModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif
	
	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// Classes
protected:
	UPROPERTY(EditAnywhere, Category = "Classes")
	TMap<FName, FStaticClass> StaticClasses;

	UPROPERTY(VisibleAnywhere, Category = "Classes")
	TMap<FString, UClass*> ClassMappings;

public:
	UFUNCTION(BlueprintCallable)
	void AddStaticClass(const FName InName, const FStaticClass& InStaticClass);

	UFUNCTION(BlueprintPure)
	UClass* GetStaticClass(const FName InName);

public:
	UFUNCTION(BlueprintPure)
	UClass* FindClass(const FString& InName, bool bExactClass = false);

	template<class T> 
	UClass* LoadClass(const FString& InName)
	{
		return LoadClass(T::StaticClass(), InName);
	}

	UFUNCTION(BlueprintPure)
	UClass* LoadClass(UClass* InClass, const FString& InName);

	//////////////////////////////////////////////////////////////////////////
	/// Objects
protected:
	UPROPERTY(EditAnywhere, Category = "Objects")
	TMap<FName, FStaticObject> StaticObjects;

	UPROPERTY(VisibleAnywhere, Category = "Objects")
	TMap<FString, UObject*> ObjectMappings;

public:
	UFUNCTION(BlueprintCallable)
	void AddStaticObject(const FName InName, const FStaticObject& InStaticObject);

	template<class T> 
	T* GetStaticObject(const FName InName)
	{
		return Cast<T>(GetStaticObject(T::StaticClass(), InName));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UObject* GetStaticObject(UClass* InClass, const FName InName);

public:
	template<class T> 
	T* FindObject(const FString& InName, bool bExactClass = false)
	{
		return Cast<T>(FindObject(T::StaticClass(), InName, bExactClass));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UObject* FindObject(UClass* InClass, const FString& InName, bool bExactClass = false);

	template<class T> 
	T* LoadObject(const FString& InName)
	{
		return Cast<T>(LoadObject(T::StaticClass(), InName));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UObject* LoadObject(UClass* InClass, const FString& InName);

	//////////////////////////////////////////////////////////////////////////
	/// Enums
protected:
	UPROPERTY(EditAnywhere, Category = "Enums")
	TMap<FString, FEnumMapping> EnumMappings;

public:
	UFUNCTION(BlueprintPure)
	UEnum* FindEnumByValue(const FString& InEnumName, int32 InEnumValue, bool bExactClass = false);

	UFUNCTION(BlueprintPure)
	UEnum* FindEnumByAuthoredName(const FString& InEnumName, const FString& InEnumAuthoredName, bool bExactClass = false);

	UFUNCTION(BlueprintPure)
	UEnum* FindEnumByDisplayName(const FString& InEnumName, const FString& InEnumDisplayName, bool bExactClass = false);

	UFUNCTION(BlueprintCallable)
	void AddEnumMapping(const FString& InEnumName, const FString& InOtherName);

	UFUNCTION(BlueprintCallable)
	void RemoveEnumMapping(const FString& InEnumName, const FString& InOtherName);

	//////////////////////////////////////////////////////////////////////////
	/// DataAsset
protected:
	UPROPERTY(EditAnywhere, Category = "DataAsset")
	TArray<UDataAssetBase*> DataAssets;

	UPROPERTY(VisibleAnywhere, Transient, Category = "DataAsset")
	TMap<FGameplayTag, UDataAssetBase*> DataAssetMap;
	
public:
	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"))
	bool HasDataAsset(const FGameplayTag& InTag) const;

	template<class T>
	T* GetDataAsset(const FGameplayTag& InTag = FGameplayTag()) const
	{
		const FGameplayTag& Tag = InTag.IsValid() ? InTag : Cast<UDataAssetBase>(T::StaticClass()->GetDefaultObject())->GetDataAssetTag();

		if(DataAssetMap.Contains(Tag))
		{
			return Cast<T>(DataAssetMap[Tag]);
		}
		return nullptr;
	}

	template<class T>
	T& GetDataAssetRef(const FGameplayTag& InTag = FGameplayTag()) const
	{
		if(T* Asset = GetDataAsset<T>(InTag))
		{
			return *Asset;
		}
		else
		{
			return UReferencePoolModuleStatics::GetReference<T>();
		}
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InTag"))
	UDataAssetBase* GetDataAsset(TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag = FGameplayTag()) const;

	template<class T>
	T* CreateDataAsset(const FGameplayTag& InTag = FGameplayTag())
	{
		const FGameplayTag& Tag = InTag.IsValid() ? InTag : Cast<UDataAssetBase>(T::StaticClass()->GetDefaultObject())->GetDataAssetTag();

		if(UDataAssetBase* DataAsset = NewObject<UDataAssetBase>(this, T::StaticClass()))
		{
			if(!DataAssetMap.Contains(Tag))
			{
				DataAssetMap.Add(Tag, DataAsset);
			}
			return Cast<T>(DataAsset);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DeterminesOutputType = "InClass", AutoCreateRefTerm = "InTag"))
	UDataAssetBase* CreateDataAsset(TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag = FGameplayTag());

	template<class T>
	bool RemoveDataAsset(const FGameplayTag& InTag = FGameplayTag())
	{
		const FGameplayTag& Tag = InTag.IsValid() ? InTag : Cast<UDataAssetBase>(T::StaticClass()->GetDefaultObject())->GetDataAssetTag();

		if(DataAssetMap.Contains(Tag))
		{
			if(UDataAssetBase* DataAsset = DataAssetMap[Tag])
			{
				DataAsset->ConditionalBeginDestroy();
				DataAssetMap.Remove(Tag);
			}
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InTag"))
	bool RemoveDataAsset(TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag = FGameplayTag());

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
