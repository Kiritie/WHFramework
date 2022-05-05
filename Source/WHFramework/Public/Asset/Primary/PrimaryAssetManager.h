// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PrimaryAssetBase.h"
#include "Engine/AssetManager.h"
#include "ReferencePool/ReferencePoolModuleBPLibrary.h"
#include "PrimaryAssetManager.generated.h"

UCLASS()
class WHFRAMEWORK_API UPrimaryAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	UPrimaryAssetManager();

public:
	virtual void StartInitialLoading() override;

	static UPrimaryAssetManager& Get();

protected:
	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, UPrimaryAssetBase*> AssetMap;

public:
	template<class T>
	T* LoadAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true)
	{
		return Cast<T>(LoadAsset(InPrimaryAssetId, bLogWarning));
	}
	
	UPrimaryAssetBase* LoadAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true);

	template<class T>
	T& LoadAssetRef(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true)
	{
		if(T* Asset = LoadAsset<T>(InPrimaryAssetId, bLogWarning))
		{
			return *Asset;
		}
		else
		{
			return UReferencePoolModuleBPLibrary::GetReference<T>();
		}
	}
};
