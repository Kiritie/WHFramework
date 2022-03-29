// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Character/CharacterModuleTypes.h"
#include "UObject/Interface.h"
#include "WHPlayerInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UWHPlayerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
* 
*/
class WHFRAMEWORK_API IWHPlayerInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void MoveForward(float InValue, bool b2DMode = false) = 0;

	virtual void MoveRight(float InValue, bool b2DMode = false) = 0;

	virtual void MoveUp(float InValue, bool b2DMode = false) = 0;
};
