// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AIServiceBase.h"

#include "AI/Base/AIControllerBase.h"
#include "AI/Base/AIAgentInterface.h"

UAIServiceBase::UAIServiceBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Controller = nullptr;
	Agent = nullptr;
}

bool UAIServiceBase::InitService(UBehaviorTreeComponent& OwnerComp)
{
	if(Controller == nullptr)
	{
		Controller = Cast<AAIControllerBase>(OwnerComp.GetOwner());
		if (Controller != nullptr && Agent == nullptr)
		{
			Agent = Cast<IAIAgentInterface>(Controller->GetPawn());
		}
	}
	if(Controller && Agent)
	{
		return true;	
	}
	return false;
}

void UAIServiceBase::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (!InitService(OwnerComp)) return;

	Super::OnBecomeRelevant(OwnerComp, NodeMemory);
}

void UAIServiceBase::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (!InitService(OwnerComp)) return;

	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}

void UAIServiceBase::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if(!InitService(OwnerComp)) return;

	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
}
