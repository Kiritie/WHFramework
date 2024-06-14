// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UObject/Interface.h"
#include "FSMAgentInterface.generated.h"

class UFiniteStateBase;
class UFSMComponent;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UFSMAgentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API IFSMAgentInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void OnFiniteStateRefresh(UFiniteStateBase* InCurrentState) { }

	virtual void OnFiniteStateChanged(UFiniteStateBase* InCurrentState, UFiniteStateBase* InLastState) { }

public:
	virtual UFSMComponent* GetFSMComponent() const = 0;
};
