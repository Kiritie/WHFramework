#pragma once

#include "Ability/Actor/AbilityActorDataBase.h"

#include "AbilityVitalityDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityVitalityDataBase : public UAbilityActorDataBase
{
	GENERATED_BODY()

public:
	UAbilityVitalityDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName RaceID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (TitleProperty = "AbilityClass"))
	TArray<FVitalityActionAbilityData> ActionAbilities;
};
