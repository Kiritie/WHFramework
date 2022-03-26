#pragma once

#include "Asset/Primary/Item/ItemAssetBase.h"

#include "SkillAssetBase.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API USkillAssetBase : public UItemAssetBase
{
	GENERATED_BODY()

public:
	USkillAssetBase();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESkillType SkillType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESkillMode SkillMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAbilitySkillBase> SkillClass;
};
