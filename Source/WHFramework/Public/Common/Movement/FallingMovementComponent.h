// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/MovementComponent.h"
#include "FallingMovementComponent.generated.h"

UCLASS(ClassGroup=Movement, meta=(BlueprintSpawnableComponent), HideCategories=(Velocity))
class WHFRAMEWORK_API UFallingMovementComponent : public UMovementComponent
{
	GENERATED_BODY()
	
public:
	UFallingMovementComponent(const FObjectInitializer& ObjectInitializer);

public:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float FallingSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float AcceleratedSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TEnumAsByte<ECollisionChannel> TraceChannel;

public:
	UFUNCTION(BlueprintPure)
	float GetFallingSpeed() const { return FallingSpeed; }

	UFUNCTION(BlueprintCallable)
	void SetFallingSpeed(float InFallingSpeed) { FallingSpeed = InFallingSpeed; }

	UFUNCTION(BlueprintPure)
	float GetAcceleratedSpeed() const { return AcceleratedSpeed; }

	UFUNCTION(BlueprintCallable)
	void SetAcceleratedSpeed(float InAcceleratedSpeed) { AcceleratedSpeed = InAcceleratedSpeed; }

	UFUNCTION(BlueprintPure)
	TEnumAsByte<ECollisionChannel> GetTraceChannel() const { return TraceChannel; }

	UFUNCTION(BlueprintCallable)
	void SetTraceChannel(const TEnumAsByte<ECollisionChannel>& InTraceChannel) { TraceChannel = InTraceChannel; }
};
