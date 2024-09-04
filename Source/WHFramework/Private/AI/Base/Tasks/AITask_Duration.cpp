// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/Base/Tasks/AITask_Duration.h"

UAITask_Duration::UAITask_Duration(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DurationTime = -1.f;
	RandomDeviation = 0.f;
	LocalRemainTime = 0.f;
}

void UAITask_Duration::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (!InitTask(OwnerComp)) return;

	if(LocalRemainTime != -1.f)
	{
		LocalRemainTime -= DeltaSeconds;
		if(LocalRemainTime <= 0.f)
		{
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		}
	}
}

EBTNodeResult::Type UAITask_Duration::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return Super::AbortTask(OwnerComp, NodeMemory);
}

EBTNodeResult::Type UAITask_Duration::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if(!InitTask(OwnerComp)) return EBTNodeResult::Failed;

	LocalRemainTime = DurationTime != -1.f ? DurationTime + FMath::RandRange(-RandomDeviation, RandomDeviation) : -1.f;

	return EBTNodeResult::InProgress;
}

bool UAITask_Duration::EDC_RandomDeviation() const
{
	return DurationTime != -1.f;
}
