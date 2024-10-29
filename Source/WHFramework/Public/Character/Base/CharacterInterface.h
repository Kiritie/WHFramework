// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Pawn/Base/PawnInterface.h"
#include "UObject/Interface.h"
#include "CharacterInterface.generated.h"

class USoundBase;
class UAnimMontage;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UCharacterInterface : public UPawnInterface
{
	GENERATED_BODY()
};

/**
* 
*/
class WHFRAMEWORK_API ICharacterInterface : public IPawnInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual void PlaySound(USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false) = 0;
	
	virtual void StopSound(bool bMulticast = false) = 0;

	virtual void PlayMontage(UAnimMontage* InMontage, bool bMulticast = false) = 0;
	
	virtual void PlayMontageByName(const FName InMontageName, bool bMulticast = false) = 0;

	virtual void StopMontage(UAnimMontage* InMontage, bool bMulticast = false) = 0;

	virtual void StopMontageByName(const FName InMontageName, bool bMulticast = false) = 0;

public:
	virtual class UCharacterAnimBase* GetAnim() const = 0;
};
