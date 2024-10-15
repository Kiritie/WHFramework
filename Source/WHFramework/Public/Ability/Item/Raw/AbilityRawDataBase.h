#pragma once

#include "Ability/Item/AbilityTradableItemDataBase.h"

#include "AbilityRawDataBase.generated.h"

class AAbilityRawBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityRawDataBase : public UAbilityTradableItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityRawDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityRawBase> RawClass;
};
