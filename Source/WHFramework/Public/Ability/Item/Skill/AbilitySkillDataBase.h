#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Ability/Item/AbilityTransactionItemDataBase.h"

#include "AbilitySkillDataBase.generated.h"

class AAbilityProjectileBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilitySkillDataBase : public UAbilityTransactionItemDataBase
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
