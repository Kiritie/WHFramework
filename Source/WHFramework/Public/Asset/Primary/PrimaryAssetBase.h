#pragma once

#include "Ability/Base/AbilityBase.h"

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
	static UPrimaryAssetBase* Empty;

public:
	virtual bool IsValid() const;
	
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
