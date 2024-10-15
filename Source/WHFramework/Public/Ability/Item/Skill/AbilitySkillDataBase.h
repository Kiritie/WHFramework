#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Ability/Item/AbilityTradableItemDataBase.h"

#include "AbilitySkillDataBase.generated.h"

class AAbilityProjectileBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilitySkillDataBase : public UAbilityTradableItemDataBase
{
	GENERATED_BODY()

public:
	UAbilitySkillDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESkillType SkillType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESkillMode SkillMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilityProjectileBase> ProjectileClass;
};
