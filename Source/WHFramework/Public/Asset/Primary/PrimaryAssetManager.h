// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PrimaryAssetBase.h"
#include "Engine/AssetManager.h"
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
};

