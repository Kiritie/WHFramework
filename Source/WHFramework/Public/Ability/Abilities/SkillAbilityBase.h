#pragma once

#include "ItemAbilityBase.h"

#include "SkillAbilityBase.generated.h"

/**
 * 技能Ability基类
 */
UCLASS()
class WHFRAMEWORK_API USkillAbilityBase : public UItemAbilityBase
{
	GENERATED_BODY()

public:
	USkillAbilityBase();
};