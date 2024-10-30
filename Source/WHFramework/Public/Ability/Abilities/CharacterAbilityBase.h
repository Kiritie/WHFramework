#pragma once
#include "PawnAbilityBase.h"

#include "CharacterAbilityBase.generated.h"

/**
 * 角色Ability基类
 */
UCLASS()
class WHFRAMEWORK_API UCharacterAbilityBase : public UPawnAbilityBase
{
	GENERATED_BODY()

public:
	UCharacterAbilityBase();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UAnimMontage* AnimMontage;
};
