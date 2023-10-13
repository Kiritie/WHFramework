// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Character/AbilityCharacterAnimBase.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Common/CommonBPLibrary.h"

UAbilityCharacterAnimBase::UAbilityCharacterAnimBase()
{
}

void UAbilityCharacterAnimBase::NativeHandleNotify(const FString& AnimNotifyName)
{
	Super::NativeHandleNotify(AnimNotifyName);
}

void UAbilityCharacterAnimBase::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	AAbilityCharacterBase* Character = Cast<AAbilityCharacterBase>(TryGetPawnOwner());

	if(!Character || !Character->Execute_GetAssetID(Character).IsValid() || !UCommonBPLibrary::IsPlaying()) return;

	bFalling = Character->IsFalling();

	VerticalSpeed = Character->GetMoveVelocity(false).Z;
	HorizontalSpeed = Character->GetMoveVelocity().Size();

	MoveDirection = FMath::FindDeltaAngleDegrees(Character->GetMoveDirection().ToOrientationRotator().Yaw, Character->GetActorRotation().Yaw);
}
