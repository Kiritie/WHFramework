// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AIAgentInterface.h"

#include "AI/Base/AIControllerBase.h"

// Add default functionality here for any IAIAgentInterface functions that are not pure virtual.
UAIBlackboardBase* IAIAgentInterface::GetBlackboard() const
{
	if(AAIControllerBase * AIController = GetAIController())
	{
		return AIController->GetBlackboard();
	}
	return nullptr;
}
