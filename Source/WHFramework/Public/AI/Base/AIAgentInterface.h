// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "UObject/Interface.h"
#include "AIAgentInterface.generated.h"

class AAIControllerBase;
class UAIBlackboardBase;
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

	virtual AAIControllerBase* GetAIController() const = 0;

	template<class T>
	T* GetBlackboard() const
	{
		return  Cast<T>(GetBlackboard());
	}

	UAIBlackboardBase* GetBlackboard() const;
};
