// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/Interface/CameraTrackableInterface.h"
#include "Pawn/PawnModuleTypes.h"
#include "PawnInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPawnInterface : public UCameraTrackableInterface
{
	GENERATED_BODY()
};

/**
* 
*/
class WHFRAMEWORK_API IPawnInterface : public ICameraTrackableInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void TransformTowards(FTransform InTransform, float InDuration = 1.f, bool bMulticast = false) = 0;

	virtual void RotationTowards(FRotator InRotation, float InDuration = 1.f, bool bMulticast = false) = 0;

	virtual void AIMoveTo(FVector InLocation, float InStopDistance = 10.f, bool bMulticast = false) = 0;

	virtual void StopAIMove(bool bMulticast = false) = 0;

public:
	virtual FName GetNameP() const = 0;
	
	virtual void SetNameP(FName InName) = 0;

	virtual AController* GetDefaultController() const = 0;
};
