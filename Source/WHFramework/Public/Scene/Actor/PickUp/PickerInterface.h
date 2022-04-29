// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PickerInterface.generated.h"

class APickUp;
// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPickerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IPickerInterface
{
	GENERATED_BODY()

public:
	virtual void PickUp(APickUp* InPickUp) = 0;
};
