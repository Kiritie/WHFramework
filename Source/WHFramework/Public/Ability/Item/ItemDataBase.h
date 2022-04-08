#pragma once

#include "Asset/Primary/PrimaryAssetBase.h"

#include "ItemDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UItemDataBase : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UItemDataBase();
	
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
	static UItemDataBase* Empty;

public:	
	virtual bool EqualType(EItemType InItemType) const;
};
