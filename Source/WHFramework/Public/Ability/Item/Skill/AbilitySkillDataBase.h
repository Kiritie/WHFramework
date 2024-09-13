#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Ability/Item/AbilityItemDataBase.h"

#include "AbilitySkillDataBase.generated.h"

class AAbilitySkillBase;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilitySkillDataBase : public UAbilityItemDataBase
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
	TSubclassOf<AAbilitySkillBase> SkillClass;
};
