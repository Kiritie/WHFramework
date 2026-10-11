#include "AI/Base/Tasks/AITask_ResetData.h"
#include "AI/Base/AIBlackboardContext.h"
#include "AI/Base/AIControllerBase.h"
#include "BehaviorTree/BehaviorTree.h"

UAITask_ResetData::UAITask_ResetData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bNotifyTick = false;
}

void UAITask_ResetData::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	if (Asset.BlackboardAsset)
	{
		for (FBlackboardKeySelector& Key : ResetKeys)
		{
			Key.ResolveSelectedKey(*Asset.BlackboardAsset);
		}
	}
}

EBTNodeResult::Type UAITask_ResetData::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return Super::AbortTask(OwnerComp, NodeMemory);
}

EBTNodeResult::Type UAITask_ResetData::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (Super::ExecuteTask(OwnerComp, NodeMemory) == EBTNodeResult::Failed)
	{
		return EBTNodeResult::Failed;
	}
	UAIBlackboardContext* Context = Controller->GetBlackboard();
	if (!Context)
	{
		return EBTNodeResult::Failed;
	}
	for (const FBlackboardKeySelector& Key : ResetKeys)
	{
		if (!Key.SelectedKeyName.IsNone())
		{
			Context->ResetValue(Key.SelectedKeyName);
		}
	}
	return EBTNodeResult::Succeeded;
}
