#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Ability/Item/AbilityTransItemDataBase.h"

#include "AbilitySkillDataBase.generated.h"

class AAbilityProjectileBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilitySkillDataBase : public UAbilityTransItemDataBase
{
	GENERATED_BODY()

public:
	UAbilitySkillDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAbilitySkillMode SkillMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityProjectileBase> ProjectileClass;
};
