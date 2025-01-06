#pragma once

#include "Ability/Item/AbilityTransItemDataBase.h"

#include "AbilityRawDataBase.generated.h"

class AAbilityRawBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityRawDataBase : public UAbilityTransItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityRawDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityRawBase> RawClass;
};
