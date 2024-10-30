#pragma once
#include "PawnActionAbilityBase.h"

#include "CharacterActionAbilityBase.generated.h"

/**
 * ��ɫ��ΪAbility����
 */
UCLASS()
class WHFRAMEWORK_API UCharacterActionAbilityBase : public UPawnActionAbilityBase
{
	GENERATED_BODY()

public:
	UCharacterActionAbilityBase();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UAnimMontage* AnimMontage;
};
