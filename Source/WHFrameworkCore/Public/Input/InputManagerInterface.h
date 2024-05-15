// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "InputTypes.h"
#include "UObject/Interface.h"
#include "InputManagerInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UInputManagerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORKCORE_API IInputManagerInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual int32 GetNativeInputPriority() const = 0;
	
	virtual EInputMode GetNativeInputMode() const = 0;
	
	virtual void SetNativeInputMode(EInputMode InInputMode) { }
};
