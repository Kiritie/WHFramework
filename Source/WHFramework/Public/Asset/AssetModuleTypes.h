#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "AssetModuleTypes.generated.h"

class UPrimaryAssetBase;

USTRUCT(BlueprintType)
struct FPrimaryAssets
{
	GENERATED_BODY()

public:
	FPrimaryAssets()
	{
		Assets = TMap<FPrimaryAssetId, UPrimaryAssetBase*>();
	}

	FPrimaryAssets(const TMap<FPrimaryAssetId, UPrimaryAssetBase*>& InAssets)
	{
		Assets = InAssets;
	}

public:
	UPROPERTY()
	TMap<FPrimaryAssetId, UPrimaryAssetBase*> Assets;
};
