// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetModuleTypes.h"
#include "Engine/AssetManager.h"
#include "Primary/PrimaryEntityInterface.h"
#include "ReferencePool/ReferencePoolModuleBPLibrary.h"
#include "Primary/PrimaryAssetBase.h"
#include "AssetManagerBase.generated.h"

class UPrimaryAssetBase;
UCLASS()
class WHFRAMEWORK_API UAssetManagerBase : public UAssetManager
{
	GENERATED_BODY()

public:
	UAssetManagerBase();

public:
	virtual void StartInitialLoading() override;

	static UAssetManagerBase& Get();

protected:
	UPROPERTY(Transient)
	TMap<FPrimaryAssetType, FPrimaryAssets> PrimaryAssetMap;

public:
	virtual TSharedPtr<FStreamableHandle> LoadPrimaryAsset(const FPrimaryAssetId& AssetToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority) override;
	
	virtual UPrimaryAssetBase* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true);

	template<class T>
	T* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true)
	{
		return Cast<T>(LoadPrimaryAsset(InPrimaryAssetId, bLogWarning));
	}

	template<class T>
	T& LoadPrimaryAssetRef(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true)
	{
		if(T* Asset = LoadPrimaryAsset<T>(InPrimaryAssetId, bLogWarning))
		{
			return *Asset;
		}
		else
		{
			return UReferencePoolModuleBPLibrary::GetReference<T>();
		}
	}

	virtual TSharedPtr<FStreamableHandle> LoadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority) override;

	virtual TArray<UPrimaryAssetBase*> LoadPrimaryAssets(FPrimaryAssetType InPrimaryAssetType, bool bLogWarning = true);

	template<class T>
	TArray<T*> LoadPrimaryAssets(FPrimaryAssetType InPrimaryAssetType, bool bLogWarning = true)
	{
		TArray<T*> LoadedItems;
		for(auto Iter : LoadPrimaryAssets(InPrimaryAssetType, bLogWarning))
		{
			if(T* LoadedItem = Cast<T>(Iter))
			{
				LoadedItems.Add(LoadedItem);
			}
		}
		return LoadedItems;
	}
};
