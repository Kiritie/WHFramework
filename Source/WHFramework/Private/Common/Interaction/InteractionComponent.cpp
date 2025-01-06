// Fill out your copyright notice in the Description page of Project Settings.


#include "Common/Interaction/InteractionComponent.h"

#include "Common/Interaction/InteractionAgentInterface.h"

UInteractionComponent::UInteractionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	
	UPrimitiveComponent::SetCollisionProfileName(TEXT("Interaction"));
	InitBoxExtent(FVector(100.f, 100.f, 100.f));
	OnComponentBeginOverlap.AddDynamic(this, &UInteractionComponent::OnBeginOverlap);
	OnComponentEndOverlap.AddDynamic(this, &UInteractionComponent::OnEndOverlap);
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UInteractionComponent::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(!GetInteractionAgent() || GetInteractionAgent()->GetInteractAgentType() == EInteractAgentType::None || OtherActor == GetOwner()) return;

	if(IInteractionAgentInterface* OtherInteractionAgent = Cast<IInteractionAgentInterface>(OtherActor))
	{
		OnAgentEnter(OtherInteractionAgent, GetInteractionAgent()->GetInteractAgentType() == EInteractAgentType::Passivity);
	}
}

void UInteractionComponent::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if(!GetInteractionAgent() || GetInteractionAgent()->GetInteractAgentType() == EInteractAgentType::None || OtherActor == GetOwner()) return;

	if(IInteractionAgentInterface* OtherInteractionAgent = Cast<IInteractionAgentInterface>(OtherActor))
	{
		OnAgentLeave(OtherInteractionAgent, GetInteractionAgent()->GetInteractAgentType() == EInteractAgentType::Passivity);
	}
}

bool UInteractionComponent::OnAgentEnter(IInteractionAgentInterface* InInteractionAgent, bool bPassive)
{
	if(!GetInteractionAgent()->IsInteractable(InInteractionAgent) || GetInteractionAgent()->GetOverlappingAgents().Contains(InInteractionAgent)) return false;
	if(!bPassive ? GetInteractionAgent()->GetInteractableActions(InInteractionAgent).IsEmpty() : InInteractionAgent->GetInteractableActions(GetInteractionAgent()).IsEmpty()) return false;
	
	GetInteractionAgent()->GetOverlappingAgents().Add(InInteractionAgent);
	if(!GetInteractionAgent()->GetInteractingAgent())
	{
		GetInteractionAgent()->SetInteractingAgent(InInteractionAgent);
	}
	InInteractionAgent->GetInteractionComponent()->OnAgentEnter(GetInteractionAgent(), !bPassive);
	return true;
}

bool UInteractionComponent::OnAgentLeave(IInteractionAgentInterface* InInteractionAgent, bool bPassive)
{
	if(!GetInteractionAgent()->IsInteractable(InInteractionAgent) || !GetInteractionAgent()->GetOverlappingAgents().Contains(InInteractionAgent)) return false;

	GetInteractionAgent()->GetOverlappingAgents().Remove(InInteractionAgent);
	if(GetInteractionAgent()->GetInteractingAgent() == InInteractionAgent)
	{
		GetInteractionAgent()->SetInteractingAgent(nullptr);
	}
	InInteractionAgent->GetInteractionComponent()->OnAgentLeave(GetInteractionAgent(), !bPassive);
	if(!GetInteractionAgent()->GetInteractingAgent() && GetInteractionAgent()->GetOverlappingAgents().IsValidIndex(0))
	{
		GetInteractionAgent()->SetInteractingAgent(GetInteractionAgent()->GetOverlappingAgents()[0]);
	}
	return true;
}

bool UInteractionComponent::AddInteractAction(EInteractAction InInteractAction)
{
	if(!InteractActions.Contains(InInteractAction))
	{
		InteractActions.Add(InInteractAction);
		return true;
	}
	return false;
}

bool UInteractionComponent::RemoveInteractAction(EInteractAction InInteractAction)
{
	if(InteractActions.Contains(InInteractAction))
	{
		InteractActions.Remove(InInteractAction);
		return true;
	}
	return false;
}

void UInteractionComponent::ClearInteractActions()
{
	InteractActions.Empty();
}

bool UInteractionComponent::IsInteractable() const
{
	return GetGenerateOverlapEvents();
}

void UInteractionComponent::SetInteractable(bool bValue)
{
	SetGenerateOverlapEvents(bValue);
}

IInteractionAgentInterface* UInteractionComponent::GetInteractionAgent() const
{
	return Cast<IInteractionAgentInterface>(GetOwner());
}
