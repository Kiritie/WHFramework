#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Ability/Item/ItemDataBase.h"

#include "AbilitySkillDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UAbilitySkillDataBase : public UItemDataBase
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
