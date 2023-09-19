// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Components/VoxelInteractionComponent.h"

UVoxelInteractionComponent::UVoxelInteractionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UVoxelInteractionComponent::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	Super::OnBeginOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

void UVoxelInteractionComponent::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	Super::OnEndOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex);
}

void UVoxelInteractionComponent::OnAgentEnter(IInteractionAgentInterface* InInteractionAgent)
{
	Super::OnAgentEnter(InInteractionAgent);
}

void UVoxelInteractionComponent::OnAgentLeave(IInteractionAgentInterface* InInteractionAgent)
{
	Super::OnAgentLeave(InInteractionAgent);
}
