// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AIDecoratorBase.h"

#include "AI/Base/AIAgentInterface.h"
#include "AI/Base/AIControllerBase.h"

UAIDecoratorBase::UAIDecoratorBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bNotifyTick = false;
}

bool UAIDecoratorBase::InitDecorator(UBehaviorTreeComponent& OwnerComp)
{
	Controller = Cast<AAIControllerBase>(OwnerComp.GetOwner());
	if (Controller)
	{
		Agent = Cast<IAIAgentInterface>(Controller->GetPawn());
	}
	return Controller && Agent;
}

bool UAIDecoratorBase::InitDecorator(UBehaviorTreeComponent& OwnerComp) const
{
	return const_cast<ThisClass*>(this)->InitDecorator(OwnerComp);
}

bool UAIDecoratorBase::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	return Super::CalculateRawConditionValue(OwnerComp, NodeMemory);
}

void UAIDecoratorBase::OnNodeActivation(FBehaviorTreeSearchData& SearchData)
{
	Super::OnNodeActivation(SearchData);
}

void UAIDecoratorBase::OnNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type NodeResult)
{
	Super::OnNodeDeactivation(SearchData, NodeResult);
}

void UAIDecoratorBase::OnNodeProcessed(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type& NodeResult)
{
	Super::OnNodeProcessed(SearchData, NodeResult);
}

APawn* UAIDecoratorBase::GetAgent(TSubclassOf<APawn> InClass) const
{
	return GetDeterminesOutputObject(Cast<APawn>(Agent), InClass);
}
