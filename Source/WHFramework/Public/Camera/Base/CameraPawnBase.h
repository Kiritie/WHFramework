// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UCameraComponent* FollowCamera;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName CameraName;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CameraTurnRate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CameraLookUpRate;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinCameraDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxCameraDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float InitCameraDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CameraZoomSpeed;

private:
	float TargetCameraDistance;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	void TurnCam(float InRate);

	void LookUpCam(float InRate);
	
	void ZoomCam(float InRate);

public:
	UFUNCTION(BlueprintPure)
	FName GetCameraName() const { return CameraName; }
	
	UFUNCTION(BlueprintPure)
	float GetCameraDistance() const { return TargetCameraDistance; }

	UFUNCTION(BlueprintCallable)
	void SetCameraDistance(float InCameraDistance = -1.f, bool bInstant = false);

public:	
	UFUNCTION(BlueprintPure)
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure)
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
