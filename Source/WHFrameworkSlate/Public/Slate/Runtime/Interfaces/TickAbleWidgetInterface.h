// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "TickAbleWidgetInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UTickAbleWidgetInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORKSLATE_API ITickAbleWidgetInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent)
	bool IsTickAble() const;

	UFUNCTION(BlueprintNativeEvent)
	void OnTick(float DeltaSeconds);
};
