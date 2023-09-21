// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UObject/Interface.h"
#include "AbilityPickerInterface.generated.h"

class AAbilityPickUpBase;
// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UAbilityPickerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IAbilityPickerInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent)
	bool OnPickUp(AAbilityPickUpBase* InPickUp);
};
