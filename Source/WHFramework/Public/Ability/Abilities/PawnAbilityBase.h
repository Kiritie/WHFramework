#pragma once

#include "Ability/Abilities/AbilityBase.h"

#include "PawnAbilityBase.generated.h"

class AAbilityPawnBase;

/**
 * 生命Ability基类
 */
UCLASS()
class WHFRAMEWORK_API UPawnAbilityBase : public UAbilityBase
{
	GENERATED_BODY()

public:
	UPawnAbilityBase();
};
