// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/AssetManager.h"
#include "PrimaryAssetManager.generated.h"

class UPrimaryAssetBase;

UCLASS()
class WHFRAMEWORK_API UPrimaryAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	UPrimaryAssetManager();

public:
	virtual void StartInitialLoading() override;

	static UPrimaryAssetManager& Get();

public:
	template<class T>
	T* LoadAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true)
	{
		return Cast<T>(LoadAsset(InPrimaryAssetId, bLogWarning));
	}
	
	UPrimaryAssetBase* LoadAsset(const FPrimaryAssetId& InPrimaryAssetId, bool bLogWarning = true);
};

