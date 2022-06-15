// Fill out your copyright notice in the Description page of ProjectSettings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CharacterAnim.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UCharacterAnim : public UAnimInstance
{
	GENERATED_BODY()

public:
	UCharacterAnim();

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bFlying;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bFalling;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bSwimming;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bCrouching;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MoveDirection;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float VerticalSpeed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float HorizontalSpeed;

public:
	bool IsFlying() const { return bFlying; }

	void SetFlying(bool bFlying) { this->bFlying = bFlying; }

	bool IsFalling() const { return bFalling; }

	void SetFalling(bool bFalling) { this->bFalling = bFalling; }

	bool IsSwimming() const { return bSwimming; }

	void SetSwimming(bool bSwimming) { this->bSwimming = bSwimming; }

	bool IsCrouching() const { return bCrouching; }

	void SetCrouching(bool bCrouching) { this->bCrouching = bCrouching; }

	float GetMoveDirection() const { return MoveDirection; }

	void SetMoveDirection(float MoveDirection) { this->MoveDirection = MoveDirection; }

	float GetVerticalSpeed() const { return VerticalSpeed; }

	void SetVerticalSpeed(float VerticalSpeed) { this->VerticalSpeed = VerticalSpeed; }

	float GetHorizontalSpeed() const { return HorizontalSpeed; }

	void SetHorizontalSpeed(float HorizontalSpeed) { this->HorizontalSpeed = HorizontalSpeed; }
};
