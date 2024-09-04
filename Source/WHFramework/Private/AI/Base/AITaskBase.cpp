// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AITaskBase.h"

#include "AI/Base/AIControllerBase.h"
#include "Character/Base/CharacterBase.h"
#include "BehaviorTree/BehaviorTree.h"

UAITaskBase::UAITaskBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bNotifyTick = true;
	bNotifyTaskFinished = true;

	Controller = nullptr;
	Agent = nullptr;
}

void UAITaskBase::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	// if(HasAnyFlags(EObjectFlags::RF_ClassDefaultObject)) return;
	//
	// for(TFieldIterator<FProperty> PropertyIt(GetClass(), EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
	// {
	// 	FStructProperty* Property = CastField<FStructProperty>(*PropertyIt);
	// 	if(Property && Property->Struct == FBlackboardKeySelector::StaticStruct())
	// 	{
	// 		FBlackboardKeySelector& KeySelector = ((FBlackboardKeySelector&)Property);
	// 		if(KeySelector.IsSet())
	// 		{
	// 			KeySelector.ResolveSelectedKey(*Asset.BlackboardAsset);
	// 		}
	// 	}
	// }
}

bool UAITaskBase::InitTask(UBehaviorTreeComponent& OwnerComp)
{
	Controller = Cast<AAIControllerBase>(OwnerComp.GetOwner());
	if (Controller)
	{
		Agent = Cast<IAIAgentInterface>(Controller->GetPawn());
	}
	return Controller && Agent;
}

void UAITaskBase::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	
}

EBTNodeResult::Type UAITaskBase::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return EBTNodeResult::Aborted;
}

EBTNodeResult::Type UAITaskBase::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return EBTNodeResult::Succeeded;
}

void UAITaskBase::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	
}

APawn* UAITaskBase::GetAgent(TSubclassOf<APawn> InClass) const
{
	return GetDeterminesOutputObject(Cast<APawn>(Agent), InClass);
}
