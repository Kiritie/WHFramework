// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/Movement/FollowingMovementComponent.h"

UFollowingMovementComponent::UFollowingMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoRegisterPhysicsVolumeUpdates = false;
	bComponentShouldUpdatePhysicsVolume = false;

	FollowingSpeed = 400.f;
	AcceleratedSpeed = 1000.f;
	FollowingTarget = nullptr;

	CurrentFollowingSpeed = 0.f;
}

void UFollowingMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	// skip if we don't want component updated when not rendered or if updated component can't move
	if (ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(UpdatedComponent))
	{
		return;
	}
	
	if (!IsValid(FollowingTarget))
	{
		return;
	}

	FVector DeltaLocation = FMath::VInterpConstantTo(GetOwner()->GetActorLocation(), FollowingTarget->GetActorLocation(), DeltaTime, CurrentFollowingSpeed) - GetOwner()->GetActorLocation();
	MoveUpdatedComponent(DeltaLocation, GetOwner()->GetActorRotation(), false);

	CurrentFollowingSpeed += AcceleratedSpeed * DeltaTime;
}

void UFollowingMovementComponent::SetFollowingTarget(AActor* InFollowingTarget)
{
	FollowingTarget = InFollowingTarget;
	
	CurrentFollowingSpeed = FollowingSpeed; 
}
