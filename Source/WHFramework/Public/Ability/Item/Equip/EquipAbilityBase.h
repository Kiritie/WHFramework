#pragma once

#include "Ability/Item/ItemAbilityBase.h"
#include "EquipAbilityBase.generated.h"

/**
 * 装备Ability基类
 */
UCLASS()
class WHFRAMEWORK_API UEquipAbilityBase : public UItemAbilityBase
{
	GENERATED_BODY()

public:
	UEquipAbilityBase();
};