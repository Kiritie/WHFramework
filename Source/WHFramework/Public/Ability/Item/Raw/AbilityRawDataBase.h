#pragma once

#include "Ability/Item/AbilityPriceItemDataBase.h"

#include "AbilityRawDataBase.generated.h"

class AAbilityRawBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityRawDataBase : public UAbilityPriceItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityRawDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityRawBase> RawClass;
};
