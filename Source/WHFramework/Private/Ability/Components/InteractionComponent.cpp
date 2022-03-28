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
	if(!GetInteractionOwner() || OtherActor == GetOwner()) return;

	if(OverlappingAgent) return;
	
	if(IInteractionAgentInterface* OtherInteractionAgent = Cast<IInteractionAgentInterface>(OtherActor))
	{
		OverlappingAgent = OtherInteractionAgent;
		OnAgentEnter(OverlappingAgent);
	}
}

void UInteractionComponent::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if(!GetInteractionOwner() || OtherActor == GetOwner()) return;

	if(!OverlappingAgent) return;

	if(IInteractionAgentInterface* OtherInteractionAgent = Cast<IInteractionAgentInterface>(OtherActor))
	{
		if(OverlappingAgent == OtherInteractionAgent)
		{
			OnAgentLeave(OverlappingAgent);
			OverlappingAgent = nullptr;
		}
	}
}

void UInteractionComponent::OnAgentEnter(IInteractionAgentInterface* InInteractionAgent)
{
	InInteractionAgent->OnEnterInteract(GetInteractionOwner());
}

void UInteractionComponent::OnAgentLeave(IInteractionAgentInterface* InInteractionAgent)
{
	InInteractionAgent->OnLeaveInteract(GetInteractionAgent());
}

bool UInteractionComponent::DoInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	if(!InInteractionAgent || InInteractAction == EInteractAction::None || !InteractActions.Contains(InInteractAction) || InteractingAction != EInteractAction::None) return false;
	
	if(GetInteractionOwner()->CanInteract(InInteractionAgent, InInteractAction))
	{
		SetInteractionAgent(InInteractionAgent);
		GetInteractionOwner()->OnInteract(InInteractionAgent, InInteractAction);
	}
	return false;
}

void UInteractionComponent::AddInteractionAction(EInteractAction InInteractAction)
{
	if(!InteractActions.Contains(InInteractAction))
	{
		InteractActions.Add(InInteractAction);
	}
}

void UInteractionComponent::RemoveInteractionAction(EInteractAction InInteractAction)
{
	if(InteractActions.Contains(InInteractAction))
	{
		InteractActions.Remove(InInteractAction);
	}
}

TArray<EInteractAction> UInteractionComponent::GetValidInteractActions(IInteractionAgentInterface* InInteractionAgent) const
{
	TArray<EInteractAction> ReturnValues;
	for(auto Iter : InteractActions)
	{
		if(GetInteractionOwner()->CanInteract(InInteractionAgent, Iter))
		{
			ReturnValues.Add(Iter);
		}
	}
	return ReturnValues;
}

IInteractionAgentInterface* UInteractionComponent::GetInteractionOwner() const
{
	return Cast<IInteractionAgentInterface>(GetOwner());
}

void UInteractionComponent::SetInteractionAgent(IInteractionAgentInterface* InInteractionAgent)
{
	InteractionAgent = InInteractionAgent;
	if(InteractionAgent)
	{
		InteractionAgent->GetInteractionComponent()->InteractionAgent = GetInteractionOwner();
	}
}
