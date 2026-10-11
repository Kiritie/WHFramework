#include "Ability/Character/AbilityCharacterAnimBase.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Engine/World.h"

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
	const AAbilityCharacterBase* Character = GetOwnerCharacter<AAbilityCharacterBase>();
	if (!IsValid(Character) || !Character->GetWorld() || !Character->GetWorld()->IsGameWorld())
	{
		bFalling = false;
		VerticalSpeed = 0.f;
		HorizontalSpeed = 0.f;
		MoveDirection = 0.f;
		return;
	}
	bFalling = Character->IsFalling();
	VerticalSpeed = Character->GetMoveVelocity().Z;
	HorizontalSpeed = Character->GetMoveVelocity(true).Size();
	MoveDirection = FMath::FindDeltaAngleDegrees(Character->GetActorRotation().Yaw, Character->GetMoveDirection(true).ToOrientationRotator().Yaw);
}
