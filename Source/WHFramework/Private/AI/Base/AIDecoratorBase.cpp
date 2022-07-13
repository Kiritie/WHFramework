// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AIDecoratorBase.h"

UAIDecoratorBase::UAIDecoratorBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	
}

bool UAIDecoratorBase::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	return Super::CalculateRawConditionValue(OwnerComp, NodeMemory);
}
