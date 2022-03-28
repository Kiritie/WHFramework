#pragma once

#include "Ability/Item/ItemAbilityBase.h"

#include "SkillAbilityBase.generated.h"

/**
 * 道具Ability基类
 */
UCLASS()
class WHFRAMEWORK_API USkillAbilityBase : public UItemAbilityBase
{
	GENERATED_BODY()

public:
	USkillAbilityBase();
};