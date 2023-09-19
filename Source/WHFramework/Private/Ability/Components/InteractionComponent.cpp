// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Components/InteractionComponent.h"

#include "Ability/Interaction/InteractionAgentInterface.h"

UInteractionComponent::UInteractionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	
	UPrimitiveComponent::SetCollisionProfileName(TEXT("Interaction"));
	InitBoxExtent(FVector(100, 100, 50));
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

void UInteractionComponent::OnAgentEnter(IInteractionAgentInterface* InInteractionAgent)
{
	if(GetInteractionAgent()->GetInteractingAgent() || InInteractionAgent->GetInteractingAgent()) return;

	GetInteractionAgent()->SetInteractingAgent(InInteractionAgent);
}

void UInteractionComponent::OnAgentLeave(IInteractionAgentInterface* InInteractionAgent)
{
	if(GetInteractionAgent()->GetInteractingAgent() != InInteractionAgent) return;

	GetInteractionAgent()->SetInteractingAgent(nullptr);
}

void UInteractionComponent::AddInteractAction(EInteractAction InInteractAction)
{
	if(!InteractActions.Contains(InInteractAction))
	{
		InteractActions.Add(InInteractAction);
	}
}

void UInteractionComponent::RemoveInteractAction(EInteractAction InInteractAction)
{
	if(InteractActions.Contains(InInteractAction))
	{
		InteractActions.Remove(InInteractAction);
	}
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
