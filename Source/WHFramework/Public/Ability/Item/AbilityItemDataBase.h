#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Asset/Primary/PrimaryAssetBase.h"

#include "AbilityItemDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityItemDataBase : public UPrimaryAssetBase
{
	GENERATED_BODY()

public:
	UAbilityItemDataBase();
	
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
	virtual bool EqualType(EAbilityItemType InItemType) const;
};
