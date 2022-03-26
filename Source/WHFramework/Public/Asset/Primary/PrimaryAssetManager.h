// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/AssetManager.h"
#include "PrimaryAssetManager.generated.h"

class UItemAssetBase;

UCLASS()
class ACTIONRPG_API UPrimaryAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	UPrimaryAssetManager();

public:
	virtual void StartInitialLoading() override;

	static UPrimaryAssetManager& Get();

public:
	template<class T = UItemAssetBase>
	T& LoadItemAsset(const FPrimaryAssetId& PrimaryAssetId, bool bLogWarning = true)
	{
		return static_cast<T>(LoadItemAsset(PrimaryAssetId, bLogWarning));
	}
	
	UItemAssetBase& LoadItemAsset(const FPrimaryAssetId& PrimaryAssetId, bool bLogWarning = true);
};

