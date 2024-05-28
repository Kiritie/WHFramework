// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/Base/Tasks/AITask_ResetData.h"

#include "AI/Base/AIBlackboardBase.h"
#include "AI/Base/AIControllerBase.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeTypes.h"

UAITask_ResetData::UAITask_ResetData(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bNotifyTick = false;
}

void UAITask_ResetData::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	for(auto& Iter : ResetKeys)
	{
		Iter.ResolveSelectedKey(*Asset.BlackboardAsset);
	}
}

EBTNodeResult::Type UAITask_ResetData::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return Super::AbortTask(OwnerComp, NodeMemory);
}

EBTNodeResult::Type UAITask_ResetData::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (!InitTask(OwnerComp)) return EBTNodeResult::Failed;

	for(auto& Iter : ResetKeys)
	{
		if(!Iter.SelectedKeyName.IsNone())
		{
			Controller->GetBlackboard()->ResetValue(Iter.SelectedKeyName);
		}
	}
	
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}
