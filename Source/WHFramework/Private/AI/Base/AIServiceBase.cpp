// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AIServiceBase.h"

#include "AI/Base/AIControllerBase.h"
#include "Character/Base/CharacterBase.h"

UAIServiceBase::UAIServiceBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	OwnerController = nullptr;
	OwnerCharacter = nullptr;
}

bool UAIServiceBase::InitService(UBehaviorTreeComponent& OwnerComp)
{
	if(OwnerController == nullptr)
	{
		OwnerController = Cast<AAIControllerBase>(OwnerComp.GetOwner());
		if (OwnerController != nullptr && OwnerCharacter == nullptr)
		{
			OwnerCharacter = Cast<ACharacterBase>(OwnerController->GetPawn());
		}
	}
	if(OwnerController && OwnerCharacter)
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
