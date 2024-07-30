// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/MovementComponent.h"
#include "FollowingMovementComponent.generated.h"

UCLASS(ClassGroup=Movement, meta=(BlueprintSpawnableComponent), HideCategories=(Velocity))
class WHFRAMEWORK_API UFollowingMovementComponent : public UMovementComponent
{
	GENERATED_UCLASS_BODY()

public:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float FollowingSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float AcceleratedSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	AActor* FollowingTarget;

private:
	float CurrentFollowingSpeed;

public:
	UFUNCTION(BlueprintPure)
	float GetFollowingSpeed() const { return FollowingSpeed; }

	UFUNCTION(BlueprintCallable)
	void SetFollowingSpeed(float InFollowingSpeed) { FollowingSpeed = InFollowingSpeed; }

	UFUNCTION(BlueprintPure)
	float GetAcceleratedSpeed() const { return AcceleratedSpeed; }

	UFUNCTION(BlueprintCallable)
	void SetAcceleratedSpeed(float InAcceleratedSpeed) { AcceleratedSpeed = InAcceleratedSpeed; }

	UFUNCTION(BlueprintPure)
	AActor* GetFollowingTarget() const { return FollowingTarget; }

	UFUNCTION(BlueprintCallable)
	void SetFollowingTarget(AActor* InFollowingTarget);
};



