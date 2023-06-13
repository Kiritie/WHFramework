// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Character/CharacterModuleTypes.h"
#include "UObject/Interface.h"
#include "CharacterInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UCharacterInterface : public UInterface
{
	GENERATED_BODY()
};

/**
* 
*/
class WHFRAMEWORK_API ICharacterInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void PlaySound(class USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false) = 0;
	
	virtual void StopSound(bool bMulticast = false) = 0;

	virtual void PlayMontage(class UAnimMontage* InMontage, bool bMulticast = false) = 0;
	
	virtual void PlayMontageByName(const FName InMontageName, bool bMulticast = false) = 0;

	virtual void StopMontage(class UAnimMontage* InMontage, bool bMulticast = false) = 0;

	virtual void StopMontageByName(const FName InMontageName, bool bMulticast = false) = 0;

	virtual void TransformTowards(FTransform InTransform, float InDuration = 1.f, bool bMulticast = false) = 0;

	virtual void RotationTowards(FRotator InRotation, float InDuration = 1.f, bool bMulticast = false) = 0;

	virtual void AIMoveTo(FVector InLocation, float InStopDistance = 10.f, bool bMulticast = false) = 0;

	virtual void StopAIMove(bool bMulticast = false) = 0;

public:
	virtual FName GetNameC() const = 0;
	
	virtual void SetNameC(FName InName) = 0;

	virtual class UCharacterAnim* GetAnim() const = 0;

	virtual class AController* GetDefaultController() const = 0;
};
