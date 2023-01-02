// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "UObject/Interface.h"
#include "AIAgentInterface.generated.h"

class UBehaviorTree;
// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UAIAgentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
* 
*/
class WHFRAMEWORK_API IAIAgentInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual UBehaviorTree* GetBehaviorTreeAsset() const = 0;
};
