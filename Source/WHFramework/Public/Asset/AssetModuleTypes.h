#pragma once

#include "AssetModuleTypes.generated.h"

class UPrimaryAssetBase;
/**
 *
 */
USTRUCT(BlueprintType)
struct FPrimaryAssets
{
	GENERATED_BODY()

public:
	FPrimaryAssets()
	{
		Assets = TArray<UPrimaryAssetBase*>();
	}

	FPrimaryAssets(const TArray<UPrimaryAssetBase*>& InAssets)
	{
		Assets = InAssets;
	}

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<UPrimaryAssetBase*> Assets;
};
