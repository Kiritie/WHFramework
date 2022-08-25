// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Base/CharacterAnim.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Global/GlobalBPLibrary.h"

UCharacterAnim::UCharacterAnim()
{
	bFlying = false;
	bFalling = false;
	bSwimming = false;
	bCrouching = false;
	MoveDirection = 0.f;
	VerticalSpeed = 0.f;
	HorizontalSpeed = 0.f;
}

void UCharacterAnim::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());

	if(!Character || !UGlobalBPLibrary::IsPlaying()) return;
	
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
