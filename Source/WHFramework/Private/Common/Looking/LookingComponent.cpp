// Copyright 2018-2021 Mickael Daniel. All Rights Reserved.

#include "Common/Looking/LookingComponent.h"

#include "Common/Looking/LookingAgentInterface.h"
#include "Engine/World.h"

// Sets default values for this component's properties
ULookingComponent::ULookingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;

	LookingMaxDistance = 12000.f;
	LookingRotationSpeed = 360.f;
	LookingTarget = nullptr;
	OwnerActor = nullptr;
}

// Called when the game starts
void ULookingComponent::BeginPlay()
{
	Super::BeginPlay();
	
	OwnerActor = GetOwner();
}

void ULookingComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerActor || !LookingTarget) return;

	if (!TargetIsLookAtAble(LookingTarget) || OwnerActor->GetDistanceTo(LookingTarget) > LookingMaxDistance)
	{
		TargetLookingOff();
		return;
	}

	DoLookAtTarget(LookingTarget);
}

void ULookingComponent::TargetLookingOn(AActor* InTargetActor)
{
	LookingTarget = InTargetActor;
}

void ULookingComponent::TargetLookingOff()
{
	if(OnTargetLookAtOff.IsBound())
	{
		OnTargetLookAtOff.Execute(LookingTarget);
	}
	LookingTarget = nullptr;
}

bool ULookingComponent::TargetIsLookAtAble(AActor* InTargetActor) const
{
	const bool bIsImplemented = InTargetActor->GetClass()->ImplementsInterface(ULookingAgentInterface::StaticClass());
	if (bIsImplemented)
	{
		return ILookingAgentInterface::Execute_IsLookAtAble(InTargetActor, OwnerActor);
	}
	return true;
}

bool ULookingComponent::CanLookAtTarget() const
{
	if (OnCanLookAtTarget.IsBound())
	{
		return OnCanLookAtTarget.Execute();
	}
	return true;
}

bool ULookingComponent::DoLookAtTarget(AActor* InTargetActor)
{
	if (CanLookAtTarget())
	{
		if(OnTargetLookAtOn.IsBound())
		{
			OnTargetLookAtOn.Execute(InTargetActor);
		}

		const FRotator CurrentRotation = OwnerActor->GetActorRotation();
		const FRotator TargetRotation = FRotator(CurrentRotation.Pitch, GetLookingRotation(InTargetActor).Yaw, CurrentRotation.Roll);
		if(!CurrentRotation.Equals(TargetRotation))
		{
			OwnerActor->SetActorRotation(FMath::RInterpConstantTo(CurrentRotation, TargetRotation, GetWorld()->GetDeltaSeconds(), LookingRotationSpeed));
			return false;
		}
	}
	else
	{
		if(OnTargetLookAtOff.IsBound())
		{
			OnTargetLookAtOff.Execute(InTargetActor);
		}
	}
	return true;
}

FRotator ULookingComponent::GetLookingRotation(AActor* InTargetActor) const
{
	if(!InTargetActor) InTargetActor = LookingTarget;
	const FVector TargetDirection = InTargetActor->GetActorLocation() - OwnerActor->GetActorLocation();
	return TargetDirection.ToOrientationRotator();
}
