#pragma once

#include "Asset/AssetModuleTypes.h"

#include "PrimaryAssetBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UPrimaryAssetBase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPrimaryAssetBase();
	
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FPrimaryAssetType Type;

public:
	virtual bool IsValid() const;
	
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	virtual bool EqualID(const FPrimaryAssetId& InAssetID) const;
};
