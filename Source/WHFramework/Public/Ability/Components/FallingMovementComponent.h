// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/MovementComponent.h"
#include "FallingMovementComponent.generated.h"

UCLASS(ClassGroup=Movement, meta=(BlueprintSpawnableComponent), HideCategories=(Velocity))
class WHFRAMEWORK_API UFallingMovementComponent : public UMovementComponent
{
	GENERATED_UCLASS_BODY()

public:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FallingSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AcceleratedSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<ECollisionChannel> TraceChannel;
};



