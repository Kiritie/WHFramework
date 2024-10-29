#pragma once

#include "Ability/Item/AbilityTransactionItemDataBase.h"

#include "AbilityRawDataBase.generated.h"

class AAbilityRawBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityRawDataBase : public UAbilityTransactionItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityRawDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityRawBase> RawClass;
};
