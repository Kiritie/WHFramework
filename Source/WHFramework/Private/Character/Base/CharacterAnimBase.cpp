// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Base/CharacterAnimBase.h"

#include "Character/Base/CharacterBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Common/CommonStatics.h"

UCharacterAnimBase::UCharacterAnimBase()
{
	bFlying = false;
	bFalling = false;
	bSwimming = false;
	bCrouching = false;
	MoveDirection = 0.f;
	VerticalSpeed = 0.f;
	HorizontalSpeed = 0.f;
}

bool UCharacterAnimBase::HandleNotify(const FAnimNotifyEvent& AnimNotifyEvent)
{
	const FString NotifyName = AnimNotifyEvent.GetNotifyEventName().ToString().Mid(11);
	NativeHandleNotify(NotifyName);
	return false;
}

void UCharacterAnimBase::NativeHandleNotify(const FString& AnimNotifyName)
{

}

void UCharacterAnimBase::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	ACharacterBase* Character = GetOwnerCharacter<ACharacterBase>();

	if(!Character || !UCommonStatics::IsPlaying()) return;
	
	UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	
	if(!MovementComponent) return;

	bFlying = MovementComponent->IsFlying();
	bFalling = MovementComponent->IsFalling();
	bSwimming = MovementComponent->IsSwimming();
	bCrouching = MovementComponent->IsCrouching();

	MoveDirection = FMath::FindDeltaAngleDegrees(Character->GetVelocity().ToOrientationRotator().Yaw, Character->GetActorRotation().Yaw);

	VerticalSpeed = Character->GetVelocity().Z;
	HorizontalSpeed = FVector(Character->GetVelocity().X, Character->GetVelocity().Y, 0.f).Size();
}

ACharacterBase* UCharacterAnimBase::GetOwnerCharacter(TSubclassOf<ACharacterBase> InClass) const
{
	return GetDeterminesOutputObject(Cast<ACharacterBase>(TryGetPawnOwner()), InClass);
}
