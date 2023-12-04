// Fill out your copyright notice in the Description page of Project Settings.


#include "Pawn/PawnModuleNetworkComponent.h"

#include "Pawn/Base/PawnInterface.h"

UPawnModuleNetworkComponent::UPawnModuleNetworkComponent()
{
	
}

bool UPawnModuleNetworkComponent::ServerTransformTowardsToMulticast_Validate(AActor* InPawn, FTransform InTransform) { return true; }
void UPawnModuleNetworkComponent::ServerTransformTowardsToMulticast_Implementation(AActor* InPawn, FTransform InTransform)
{
	if(IPawnInterface* Pawn = Cast<IPawnInterface>(InPawn))
	{
		Pawn->TransformTowards(InTransform, true);
	}
}

bool UPawnModuleNetworkComponent::ServerRotationTowardsMulticast_Validate(AActor* InPawn, FRotator InRotation, float InDuration) { return true; }
void UPawnModuleNetworkComponent::ServerRotationTowardsMulticast_Implementation(AActor* InPawn, FRotator InRotation, float InDuration)
{
	if(IPawnInterface* Pawn = Cast<IPawnInterface>(InPawn))
	{
		Pawn->RotationTowards(InRotation, InDuration, true);
	}
}

bool UPawnModuleNetworkComponent::ServerAIMoveToMulticast_Validate(AActor* InPawn, FVector InLocation, float InStopDistance) { return true; }
void UPawnModuleNetworkComponent::ServerAIMoveToMulticast_Implementation(AActor* InPawn, FVector InLocation, float InStopDistance)
{
	if(IPawnInterface* Pawn = Cast<IPawnInterface>(InPawn))
	{
		Pawn->AIMoveTo(InLocation, InStopDistance, true);
	}
}

bool UPawnModuleNetworkComponent::ServerStopAIMoveMulticast_Validate(AActor* InPawn) { return true; }
void UPawnModuleNetworkComponent::ServerStopAIMoveMulticast_Implementation(AActor* InPawn)
{
	if(IPawnInterface* Pawn = Cast<IPawnInterface>(InPawn))
	{
		Pawn->StopAIMove(true);
	}
}
