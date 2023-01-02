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

/**
 * 数据表项
 */
USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FDataTableRowBase : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	FName ID;

	FORCEINLINE FDataTableRowBase()
	{
		ID = NAME_None;
	}

public:
	FORCEINLINE bool IsValid() const
	{
		return !ID.IsNone();
	}

	virtual void OnInitializeRow(const FName& InRowName);
};
