// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AITaskBase.h"

#include "AI/Base/AIControllerBase.h"
#include "Character/Base/CharacterBase.h"
#include "BehaviorTree/BehaviorTree.h"


UAITaskBase::UAITaskBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bNotifyTick = true;
	bNotifyTaskFinished = true;

	OwnerController = nullptr;
	OwnerCharacter = nullptr;
}

void UAITaskBase::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	//for(TFieldIterator<FProperty> PropertyIt(GetClass(), EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
	//{
	//	FStructProperty* Property = CastField<FStructProperty>(*PropertyIt);
	//	if(Property && Property->Struct == FBlackboardKeySelector::StaticStruct())
	//	{
	//		((FBlackboardKeySelector&)Property).ResolveSelectedKey(*Asset.BlackboardAsset);
	//	}
	//}
}

bool UAITaskBase::InitTask(UBehaviorTreeComponent& OwnerComp)
{
	OwnerController = Cast<AAIControllerBase>(OwnerComp.GetOwner());
	if (OwnerController)
	{
		OwnerCharacter = Cast<ACharacterBase>(OwnerController->GetPawn());
	}
	return OwnerController && OwnerCharacter;
}

void UAITaskBase::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (!InitTask(OwnerComp)) return;

}

EBTNodeResult::Type UAITaskBase::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (!InitTask(OwnerComp)) return EBTNodeResult::Failed;

	return EBTNodeResult::Aborted;
}

EBTNodeResult::Type UAITaskBase::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if(!InitTask(OwnerComp)) return EBTNodeResult::Failed;

	return EBTNodeResult::Succeeded;
}

void UAITaskBase::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	if(!InitTask(OwnerComp)) return;

}
