// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/CameraModuleTypes.h"
#include "Pawn/PawnModuleTypes.h"
#include "UObject/Interface.h"
#include "CameraTrackableInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UCameraTrackableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
* 
*/
class WHFRAMEWORK_API ICameraTrackableInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	FVector GetCameraOffset() const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	float GetCameraDistance() const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	float GetCameraMinPitch() const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	float GetCameraMaxPitch() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	ECameraTrackMode GetCameraTrackMode() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	ECameraSmoothMode GetCameraSmoothMode() const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	ECameraControlMode GetCameraControlMode() const;
};
