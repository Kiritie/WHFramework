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
	TMap<FPrimaryAssetId, UPrimaryAssetBase*> PrimaryAssetMap;

	UPROPERTY(Transient)
	TMap<FPrimaryAssetType, FPrimaryAssets> PrimaryAssetsMap;

public:
	virtual TSharedPtr<FStreamableHandle> LoadPrimaryAsset(const FPrimaryAssetId& AssetToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority) override;
	
	virtual UPrimaryAssetBase* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true);

	template<class T>
	T* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true)
	{
		return Cast<T>(LoadPrimaryAsset(InPrimaryAssetId, bLogWarning));
	}
		
	template<class T>
	T* LoadPrimaryAsset(IPrimaryEntityInterface* InPrimaryEntity, bool bLogWarning = true)
	{
		if(!InPrimaryEntity) return nullptr;
		
		return LoadPrimaryAsset<T>(InPrimaryEntity->GetAssetID(), bLogWarning);
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

	template<class T>
	T& LoadPrimaryAssetRef(IPrimaryEntityInterface* InPrimaryEntity, bool bLogWarning = true)
	{
		if(!InPrimaryEntity) return UReferencePoolModuleBPLibrary::GetReference<T>();
		
		return LoadPrimaryAssetRef<T>(InPrimaryEntity->GetAssetID(), bLogWarning);
	}

	virtual TSharedPtr<FStreamableHandle> LoadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority) override;

	virtual TArray<UPrimaryAssetBase*> LoadPrimaryAssets(FPrimaryAssetType InPrimaryAssetType, bool bLogWarning = true);

	template<class T>
	TArray<T*> LoadPrimaryAssets(FPrimaryAssetType InPrimaryAssetType, bool bLogWarning = true)
	{
		TArray<T*> LoadedItems;
		for(auto Iter : LoadPrimaryAssets(InPrimaryAssetType, bLogWarning))
		{
			LoadedItems.Add(Cast<T>(Iter));
		}
		return LoadedItems;
	}

	template<class T>
	TArray<T>& LoadPrimaryAssetRefs(FPrimaryAssetType InPrimaryAssetType, bool bLogWarning = true)
	{
		static TArray<T> LoadedItems;
		for(auto Iter : LoadPrimaryAssets(InPrimaryAssetType, bLogWarning))
		{
			LoadedItems.Add(static_cast<T&>(*Iter));
		}
		return LoadedItems;
	}
};
