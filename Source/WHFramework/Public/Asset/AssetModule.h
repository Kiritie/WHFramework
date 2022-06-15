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

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetDataAsset", DeterminesOutputType = "InDataAssetClass"))
	UDataAssetBase* K2_GetDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName = NAME_None) const;

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

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateDataAsset", DeterminesOutputType = "InDataAssetClass"))
	UDataAssetBase* K2_CreateDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName = NAME_None);

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

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RemoveDataAsset"))
	bool K2_RemoveDataAsset(TSubclassOf<UDataAssetBase> InDataAssetClass, FName InDataAssetName = NAME_None);

	UFUNCTION(BlueprintCallable)
	void RemoveAllDataAsset();
};
