#pragma once

#include "Asset/Primary/PrimaryAssetBase.h"

#include "ItemAssetBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UItemAssetBase : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UItemAssetBase();
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Detail;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UTexture2D* Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Price;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxLevel;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<class UItemAbilityBase> AbilityClass;

public:
	static UItemAssetBase* Empty;

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
