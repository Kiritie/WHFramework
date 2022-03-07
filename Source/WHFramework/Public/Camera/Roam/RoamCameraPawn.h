// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/Base/CameraPawnBase.h"
#include "RoamCameraPawn.generated.h"

UCLASS()
class WHFRAMEWORK_API ARoamCameraPawn : public ACameraPawnBase
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ARoamCameraPawn();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void MoveForward(float InValue);

	virtual void MoveRight(float InValue);

	virtual void MoveUp(float InValue);
};
