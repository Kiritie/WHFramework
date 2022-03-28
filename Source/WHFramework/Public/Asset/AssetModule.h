// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Ability/AbilityModuleTypes.h"
#include "Data/DataAssetBase.h"

#include "Main/Base/ModuleBase.h"

#include "AssetModule.generated.h"

UCLASS()
class WHFRAMEWORK_API AAssetModule : public AModuleBase
{
	GENERATED_BODY()
	
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
	template<class T>
	bool HasDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass = T::StaticClass()) const
	{
		if(!InDataAssetClass) return false;

		const FName DataAssetName = InDataAssetClass.GetDefaultObject()->GetDataAssetName();
		return DataAssetMap.Contains(DataAssetName);
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "HasDataAsset"))
	bool K2_HasDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass) const;

	template<class T>
	T* GetDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass = T::StaticClass()) const
	{
		if(!InDataAssetClass) return nullptr;

		const FName DataAssetName = InDataAssetClass.GetDefaultObject()->GetDataAssetName();
		if(DataAssetMap.Contains(DataAssetName))
		{
			return Cast<T>(DataAssetMap[DataAssetName]);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetDataAsset", DeterminesOutputType = "InDataAssetClass"))
	UDataAssetBase* K2_GetDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass) const;

	template<class T>
	T* GetDataAssetByName(FName InName) const
	{
		if(DataAssetMap.Contains(InName))
		{
			return Cast<T>(DataAssetMap[InName]);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetDataAssetByName", DeterminesOutputType = "InDataAssetClass"))
	UDataAssetBase* K2_GetDataAssetByName(FName InName, TSubclassOf<UDataAssetBase> InDataAssetClass) const;

	template<class T>
	T* CreateDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass = T::StaticClass())
	{
		if(!InDataAssetClass) return nullptr;
		
		const FName DataAssetName = InDataAssetClass.GetDefaultObject()->GetDataAssetName();
		
		if(UDataAssetBase* DataAsset = NewObject<UDataAssetBase>(this, InDataAssetClass))
		{
			if(!DataAssetMap.Contains(DataAssetName))
			{
				DataAssetMap.Add(DataAssetName, DataAsset);
			}
			return Cast<T>(DataAsset);
		}
		return nullptr;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateDataAsset", DeterminesOutputType = "InDataAssetClass"))
	UDataAssetBase* K2_CreateDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass);

	template<class T>
	bool RemoveDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass = T::StaticClass())
	{
		if(!InDataAssetClass) return false;

		const FName DataAssetName = InDataAssetClass.GetDefaultObject()->GetDataAssetName();
		if(DataAssetMap.Contains(DataAssetName))
		{
			if(UDataAssetBase* DataAsset = DataAssetMap[DataAssetName])
			{
				DataAsset->ConditionalBeginDestroy();
				DataAssetMap.Remove(DataAssetName);
			}
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RemoveDataAsset"))
	bool K2_RemoveDataAsset(TSubclassOf<UDataAssetBase>  InDataAssetClass);

	UFUNCTION(BlueprintCallable)
	void RemoveAllDataAsset();
};
