// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/AssetManager.h"
#include "ReferencePool/ReferencePoolModuleBPLibrary.h"
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

public:
	virtual TSharedPtr<FStreamableHandle> LoadPrimaryAsset(const FPrimaryAssetId& AssetToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority) override;
	
	template<class T>
	T* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true)
	{
		return Cast<T>(LoadPrimaryAsset(InPrimaryAssetId, bLogWarning));
	}
	
	UPrimaryAssetBase* LoadPrimaryAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true);

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
};
