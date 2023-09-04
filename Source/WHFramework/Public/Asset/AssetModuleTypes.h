#pragma once

#include "Engine/DataTable.h"
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
		Assets = TMap<FPrimaryAssetId, UPrimaryAssetBase*>();
	}

	FPrimaryAssets(const TMap<FPrimaryAssetId, UPrimaryAssetBase*>& InAssets)
	{
		Assets = InAssets;
	}

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<FPrimaryAssetId, UPrimaryAssetBase*> Assets;
};

/**
 * 数据表项
 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FDataTableRowBase : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	FName ID;

	FDataTableRowBase()
	{
		ID = NAME_None;
	}

public:
	virtual bool IsValid() const
	{
		return !ID.IsNone();
	}

	virtual void OnInitializeRow(const FName& InRowName);
};
