// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetModuleTypes.h"
#include "Engine/AssetManager.h"
#include "ReferencePool/ReferencePoolModuleStatics.h"
#include "Primary/PrimaryAssetBase.h"
#include "AssetManagerBase.generated.h"

class UPrimaryAssetBase;
UCLASS()
class WHFRAMEWORK_API UAssetManagerBase : public UAssetManager
{
	GENERATED_BODY()

public:
	UAssetManagerBase();

	static UAssetManagerBase& Get();

public:
	virtual void StartInitialLoading() override;

	virtual void ReleaseRuntimeData();

public:
	virtual TSharedPtr<FStreamableHandle> LoadPrimaryAsset(const FPrimaryAssetId& AssetToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority) override;
	
	virtual UPrimaryAssetBase* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bEnsured = true);

	template<class T>
	T* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bEnsured = true)
	{
		return Cast<T>(LoadPrimaryAsset(InPrimaryAssetId, bEnsured));
	}

	template<class T>
	T& LoadPrimaryAssetRef(const FPrimaryAssetId& InPrimaryAssetId, bool bEnsured = true)
	{
		if(T* Asset = LoadPrimaryAsset<T>(InPrimaryAssetId, bEnsured))
		{
			return *Asset;
		}
		else
		{
			return UReferencePoolModuleStatics::GetReference<T>();
		}
	}

	virtual TSharedPtr<FStreamableHandle> LoadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority) override;

	virtual TArray<UPrimaryAssetBase*> LoadPrimaryAssets(FPrimaryAssetType InPrimaryAssetType, bool bEnsured = true);

	template<class T>
	TArray<T*> LoadPrimaryAssets(FPrimaryAssetType InPrimaryAssetType, bool bEnsured = true)
	{
		TArray<T*> LoadedItems;
		for(auto Iter : LoadPrimaryAssets(InPrimaryAssetType, bEnsured))
		{
			if(T* LoadedItem = Cast<T>(Iter))
			{
				LoadedItems.Add(LoadedItem);
			}
		}
		return LoadedItems;
	}

protected:
	UPROPERTY()
	TMap<FPrimaryAssetType, FPrimaryAssets> PrimaryAssetMap;
};
