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

	if(IInteractionAgentInterface* OtherInteractionAgent = Cast<IInteractionAgentInterface>(OtherActor))
	{
		OnAgentEnter(OtherInteractionAgent);
	}
}

void UInteractionComponent::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if(!GetInteractionOwner() || OtherActor == GetOwner()) return;

	if(IInteractionAgentInterface* OtherInteractionAgent = Cast<IInteractionAgentInterface>(OtherActor))
	{
		OnAgentLeave(OtherInteractionAgent);
	}
}

void UInteractionComponent::OnAgentEnter(IInteractionAgentInterface* InInteractionAgent)
{
	if(InteractionAgent == nullptr)
	{
		SetInteractionAgent(InInteractionAgent);
	}
}

void UInteractionComponent::OnAgentLeave(IInteractionAgentInterface* InInteractionAgent)
{
	if(InInteractionAgent == InteractionAgent)
	{
		SetInteractionAgent(nullptr);
	}
}

bool UInteractionComponent::DoInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	if(!InInteractionAgent || InInteractAction == EInteractAction::None || !InteractActions.Contains(InInteractAction) || InteractingAction != EInteractAction::None) return false;
	
	if(GetInteractionOwner()->CanInteract(InInteractionAgent, InInteractAction))
	{
		GetInteractionOwner()->OnInteract(InInteractionAgent, InInteractAction);
		InInteractionAgent->OnInteract(GetInteractionOwner(), InInteractAction);
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

void UInteractionComponent::SetInteractionAgent(IInteractionAgentInterface* InInteractionAgent)
{
	if(InInteractionAgent == InteractionAgent) return;

	if(InteractionAgent)
	{
		GetInteractionOwner()->OnLeaveInteract(InteractionAgent);
	}

	InteractionAgent = InInteractionAgent;

	if(InteractionAgent)
	{
		GetInteractionOwner()->OnEnterInteract(InteractionAgent);
	}
}

UInteractionComponent* UInteractionComponent::GetInteractionComponent() const
{
	if(InteractionAgent)
	{
		return InteractionAgent->GetInteractionComponent();
	}
	return nullptr;
}

IInteractionAgentInterface* UInteractionComponent::GetInteractionOwner() const
{
	return Cast<IInteractionAgentInterface>(GetOwner());
}
