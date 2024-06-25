#pragma once

#include "Ability/Pawn/AbilityPawnDataBase.h"

#include "AbilityCharacterDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilityCharacterDataBase : public UAbilityPawnDataBase
{
	GENERATED_BODY()

public:
	UAbilityCharacterDataBase();
};
