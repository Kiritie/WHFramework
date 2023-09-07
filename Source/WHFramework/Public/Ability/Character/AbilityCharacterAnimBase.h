// Fill out your copyright notice in the Description page of ProjectSettings.

#pragma once

#include "Character/Base/CharacterAnimBase.h"
#include "AbilityCharacterAnimBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UAbilityCharacterAnimBase : public UCharacterAnimBase
{
	GENERATED_BODY()

public:
	UAbilityCharacterAnimBase();

protected:
	virtual void NativeHandleNotify(const FString& AnimNotifyName) override;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
