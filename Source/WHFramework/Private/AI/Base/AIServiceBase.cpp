// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AIServiceBase.h"

#include "AI/Base/AIControllerBase.h"
#include "AI/Base/AIAgentInterface.h"

UAIServiceBase::UAIServiceBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
	
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

void UAIServiceBase::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
}

void UAIServiceBase::OnSearchStart(FBehaviorTreeSearchData& SearchData)
{
	Super::OnSearchStart(SearchData);
}

void UAIServiceBase::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);
}

void UAIServiceBase::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}

APawn* UAIServiceBase::GetAgent(TSubclassOf<APawn> InClass) const
{
	return GetDeterminesOutputObject(Cast<APawn>(Agent), InClass);
}
