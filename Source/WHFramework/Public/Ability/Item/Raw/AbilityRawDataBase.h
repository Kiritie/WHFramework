#pragma once

#include "Ability/Item/AbilityItemDataBase.h"

#include "AbilityRawDataBase.generated.h"

class AAbilityRawBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityRawDataBase : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityRawDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityRawBase> RawClass;
};
