// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModuleTypes.h"
#include "GameFramework/Pawn.h"
#include "CameraPawnBase.generated.h"

UCLASS()
class WHFRAMEWORK_API ACameraPawnBase : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ACameraPawnBase();

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UCameraComponent* Camera;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UPawnMovementComponent* MovementComponent;

protected:
	UPROPERTY(EditAnywhere)
	FName CameraName;

	UPROPERTY(EditAnywhere)
	ECameraCollisionMode CameraCollisionMode;

protected:
	virtual void BeginPlay() override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	virtual void Tick(float DeltaTime) override;

public:
	UFUNCTION(BlueprintPure)
	FName GetCameraName() const { return CameraName; }

	UFUNCTION(BlueprintPure)
	UCameraComponent* GetCamera() const { return Camera; }

	UFUNCTION(BlueprintPure)
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure)
	ECameraCollisionMode GetCameraCollisionMode() const { return CameraCollisionMode; }

	UFUNCTION(BlueprintCallable)
	virtual void SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode);
};
