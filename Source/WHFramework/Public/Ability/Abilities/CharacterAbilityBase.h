#pragma once
#include "Ability/Abilities/AbilityBase.h"

#include "CharacterAbilityBase.generated.h"

class AAbilityCharacterBase;
/**
 * 角色Ability基类
 */
UCLASS()
class WHFRAMEWORK_API UCharacterAbilityBase : public UAbilityBase
{
	GENERATED_BODY()

public:
	UCharacterAbilityBase();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UAnimMontage* AnimMontage;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bAutoEndAbility;
};