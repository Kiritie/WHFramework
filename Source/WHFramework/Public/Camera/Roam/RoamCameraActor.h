// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/Base/CameraActorBase.h"
#include "RoamCameraActor.generated.h"

UCLASS()
class WHFRAMEWORK_API ARoamCameraActor : public ACameraActorBase
{
	GENERATED_BODY()

public:
	ARoamCameraActor();

public:
	virtual void OnInitialize_Implementation() override;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class USphereComponent* Sphere;

public:
	virtual void SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode) override;
};
