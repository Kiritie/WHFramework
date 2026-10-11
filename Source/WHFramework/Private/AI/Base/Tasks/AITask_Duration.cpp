#include "AI/Base/Tasks/AITask_Duration.h"

UAITask_Duration::UAITask_Duration(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAITask_Duration::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	const uint64 Generation = GetTaskGeneration();
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
	if (!IsTaskActive() || GetTaskGeneration() != Generation)
	{
		return;
	}
	LocalRemainTime -= DeltaSeconds;
	if (LocalRemainTime <= 0.f)
	{
		FinishTaskOnce(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UAITask_Duration::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	LocalRemainTime = 0.f;
	return Super::AbortTask(OwnerComp, NodeMemory);
}

EBTNodeResult::Type UAITask_Duration::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (Super::ExecuteTask(OwnerComp, NodeMemory) == EBTNodeResult::Failed)
	{
		return EBTNodeResult::Failed;
	}
	if (!FMath::IsFinite(DurationTime) || DurationTime < 0.f || !FMath::IsFinite(RandomDeviation))
	{
		return EBTNodeResult::Failed;
	}
	LocalRemainTime = FMath::Max(0.f, DurationTime + FMath::FRandRange(-FMath::Max(0.f, RandomDeviation), FMath::Max(0.f, RandomDeviation)));
	return LocalRemainTime <= 0.f ? EBTNodeResult::Succeeded : EBTNodeResult::InProgress;
}

void UAITask_Duration::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	LocalRemainTime = 0.f;
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}
