#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Ability/Item/ItemDataBase.h"

#include "SkillDataBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API USkillDataBase : public UItemDataBase
{
	GENERATED_BODY()

public:
	USkillDataBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESkillType SkillType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESkillMode SkillMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilitySkillBase> SkillClass;
};
