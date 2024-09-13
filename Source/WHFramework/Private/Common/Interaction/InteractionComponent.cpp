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
	if(!GetInteractionAgent() || OtherActor == GetOwner()) return;

	if(IInteractionAgentInterface* OtherInteractionAgent = Cast<IInteractionAgentInterface>(OtherActor))
	{
		OnAgentEnter(OtherInteractionAgent);
	}
}

void UInteractionComponent::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if(!GetInteractionAgent() || OtherActor == GetOwner()) return;

	if(IInteractionAgentInterface* OtherInteractionAgent = Cast<IInteractionAgentInterface>(OtherActor))
	{
		OnAgentLeave(OtherInteractionAgent);
	}
}

bool UInteractionComponent::OnAgentEnter(IInteractionAgentInterface* InInteractionAgent)
{
	if(!GetInteractionAgent()->IsInteractable(InInteractionAgent) || GetInteractionAgent()->GetInteractableActions(InInteractionAgent).IsEmpty() || GetInteractionAgent()->GetOverlappingAgents().Contains(InInteractionAgent)) return false;
	
	GetInteractionAgent()->GetOverlappingAgents().Add(InInteractionAgent);
	if(!GetInteractionAgent()->GetInteractingAgent())
	{
		GetInteractionAgent()->SetInteractingAgent(InInteractionAgent);
	}
	InInteractionAgent->GetInteractionComponent()->OnAgentEnter(GetInteractionAgent());
	return true;
}

bool UInteractionComponent::OnAgentLeave(IInteractionAgentInterface* InInteractionAgent)
{
	if(!GetInteractionAgent()->IsInteractable(InInteractionAgent) || !GetInteractionAgent()->GetOverlappingAgents().Contains(InInteractionAgent)) return false;

	GetInteractionAgent()->GetOverlappingAgents().Remove(InInteractionAgent);
	if(GetInteractionAgent()->GetInteractingAgent() == InInteractionAgent)
	{
		GetInteractionAgent()->SetInteractingAgent(nullptr);
	}
	InInteractionAgent->GetInteractionComponent()->OnAgentLeave(GetInteractionAgent());
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
	return GetCollisionEnabled() == ECollisionEnabled::QueryOnly;
}

void UInteractionComponent::SetInteractable(bool bValue)
{
	SetCollisionEnabled(bValue ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

IInteractionAgentInterface* UInteractionComponent::GetInteractionAgent() const
{
	return Cast<IInteractionAgentInterface>(GetOwner());
}
