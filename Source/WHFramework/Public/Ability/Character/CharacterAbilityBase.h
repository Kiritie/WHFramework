#pragma once
#include "Ability/Base/AbilityBase.h"

#include "CharacterAbilityBase.generated.h"

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

public:
	UFUNCTION(BlueprintPure)
	class AAbilityCharacterBase* GetOwnerCharacter() const;
};