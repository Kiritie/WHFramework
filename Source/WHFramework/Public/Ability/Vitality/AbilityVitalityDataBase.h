#pragma once

#include "Ability/Item/AbilityItemDataBase.h"

#include "AbilityVitalityDataBase.generated.h"

class AAbilityVitalityBase;
UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityVitalityDataBase : public UAbilityItemDataBase
{
	GENERATED_BODY()

public:
	UAbilityVitalityDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Range;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 EXP;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 BaseEXP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 EXPFactor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityVitalityBase> Class;
};
