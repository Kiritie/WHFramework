// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/Base/CameraPawnBase.h"
#include "Gameplay/WHPlayerInterface.h"
#include "RoamCameraPawn.generated.h"

UCLASS()
class WHFRAMEWORK_API ARoamCameraPawn : public ACameraPawnBase, public IWHPlayerInterface
{
	GENERATED_BODY()

public:
	ARoamCameraPawn();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USphereComponent* Sphere;

protected:
	virtual void BeginPlay() override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:	
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void MoveForward(float InValue, bool b2DMode = false) override;

	virtual void MoveRight(float InValue, bool b2DMode = false) override;

	virtual void MoveUp(float InValue, bool b2DMode = false) override;

public:
	virtual void SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode) override;
};
