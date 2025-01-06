#pragma once

#include "Engine/DataAsset.h"

#include "PrimaryAssetBase.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UPrimaryAssetBase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPrimaryAssetBase();

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void OnInitialize();
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void OnReset();
	
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FPrimaryAssetType Type;

public:
	virtual bool IsValid() const;
	
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
