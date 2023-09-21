// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/Base/CameraPawnBase.h"
#include "RoamCameraPawn.generated.h"

UCLASS()
class WHFRAMEWORK_API ARoamCameraPawn : public ACameraPawnBase
{
	GENERATED_BODY()

public:
	ARoamCameraPawn();

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class USphereComponent* Sphere;

protected:
	virtual void MoveForward_Implementation(float InValue) override;

	virtual void MoveRight_Implementation(float InValue) override;

	virtual void MoveUp_Implementation(float InValue) override;

public:
	virtual void SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode) override;
};
