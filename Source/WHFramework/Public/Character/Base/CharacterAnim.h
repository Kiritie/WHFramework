// Fill out your copyright notice in the Description page of ProjectSettings.

#pragma once

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
	virtual bool HandleNotify(const FAnimNotifyEvent& AnimNotifyEvent) override;

	virtual void NativeHandleNotify(const FString& AnimNotifyName);

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

	void SetFlying(bool bInFlying) { this->bFlying = bInFlying; }

	bool IsFalling() const { return bFalling; }

	void SetFalling(bool bInFalling) { this->bFalling = bInFalling; }

	bool IsSwimming() const { return bSwimming; }

	void SetSwimming(bool bInSwimming) { this->bSwimming = bInSwimming; }

	bool IsCrouching() const { return bCrouching; }

	void SetCrouching(bool bInCrouching) { this->bCrouching = bInCrouching; }

	float GetMoveDirection() const { return MoveDirection; }

	void SetMoveDirection(float InMoveDirection) { this->MoveDirection = InMoveDirection; }

	float GetVerticalSpeed() const { return VerticalSpeed; }

	void SetVerticalSpeed(float InVerticalSpeed) { this->VerticalSpeed = InVerticalSpeed; }

	float GetHorizontalSpeed() const { return HorizontalSpeed; }

	void SetHorizontalSpeed(float InHorizontalSpeed) { this->HorizontalSpeed = InHorizontalSpeed; }
};
