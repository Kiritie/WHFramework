// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "TickAbleWidgetInterface.h"
#include "Parameter/ParameterTypes.h"

#include "UObject/Interface.h"
#include "BaseWidgetInterface.generated.h"

class UWidget;
// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UBaseWidgetInterface : public UTickAbleWidgetInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORKSLATE_API IBaseWidgetInterface : public ITickAbleWidgetInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.

public:
	virtual void OnRefresh() = 0;

public:
	virtual void Refresh() = 0;

	virtual void Destroy(bool bRecovery = false) = 0;
};
