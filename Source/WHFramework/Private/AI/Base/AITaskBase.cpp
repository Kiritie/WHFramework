#include "AI/Base/AITaskBase.h"
#include "AI/Base/AIControllerBase.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameFramework/Pawn.h"

UAITaskBase::UAITaskBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNodeInstance = true;
	bNotifyTick = true;
	bNotifyTaskFinished = true;
}

void UAITaskBase::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
}

bool UAITaskBase::InitTask(UBehaviorTreeComponent& OwnerComp)
{
	Controller = Cast<AAIControllerBase>(OwnerComp.GetAIOwner());
	Agent = Controller.IsValid() ? Controller->GetPawn() : nullptr;
	return Agent.IsValid() && Agent->HasAuthority() && GetAgent() != nullptr;
}

void UAITaskBase::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (bTaskActive && !InitTask(OwnerComp))
	{
		FinishTaskOnce(OwnerComp, EBTNodeResult::Failed);
	}
}

EBTNodeResult::Type UAITaskBase::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	++TaskGeneration;
	bTaskActive = InitTask(OwnerComp);
	return bTaskActive ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

EBTNodeResult::Type UAITaskBase::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	++TaskGeneration;
	bTaskActive = false;
	return EBTNodeResult::Aborted;
}

void UAITaskBase::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	++TaskGeneration;
	bTaskActive = false;
	Controller.Reset();
	Agent.Reset();
}

bool UAITaskBase::FinishTaskOnce(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type InResult)
{
	if (!bTaskActive || !IsValid(&OwnerComp))
	{
		return false;
	}
	++TaskGeneration;
	bTaskActive = false;
	FinishLatentTask(OwnerComp, InResult);
	return true;
}

APawn* UAITaskBase::GetAgent(TSubclassOf<APawn> InClass) const
{
	APawn* Pawn = Agent.Get();
	return Pawn && (!InClass || Pawn->IsA(InClass)) ? Pawn : nullptr;
}

bool UAITaskBase::FinishTaskOnce(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type InResult, uint64 ExpectedGeneration)
{
	return TaskGeneration == ExpectedGeneration && FinishTaskOnce(OwnerComp, InResult);
}
