// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/Movement/FallingMovementComponent.h"

#include "Kismet/KismetSystemLibrary.h"

UFallingMovementComponent::UFallingMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoRegisterPhysicsVolumeUpdates = false;
	bComponentShouldUpdatePhysicsVolume = false;

	FallingSpeed = 150.f;
	AcceleratedSpeed = 300.f;
	
	TraceChannel = ECC_MAX;
}


void UFallingMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
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
	
	FHitResult HitResult;
	FVector RayStart = GetOwner()->GetActorLocation();
	FVector RayEnd = FVector(RayStart.X, RayStart.Y, 0.f);
	FVector Extents = GetOwner()->GetComponentsBoundingBox(true, true).GetExtent();
	if(UKismetSystemLibrary::BoxTraceSingle(this, RayStart, RayEnd, Extents, GetOwner()->GetActorRotation(), UEngineTypes::ConvertToTraceType(TraceChannel), false, {}, EDrawDebugTrace::None, HitResult, true))
	{
		FVector DeltaLocation = FMath::VInterpConstantTo(GetOwner()->GetActorLocation(), HitResult.Location, DeltaTime, FallingSpeed) - GetOwner()->GetActorLocation();
		MoveUpdatedComponent(DeltaLocation, GetOwner()->GetActorRotation(), false);

		FallingSpeed += AcceleratedSpeed * DeltaTime;
	}
}

