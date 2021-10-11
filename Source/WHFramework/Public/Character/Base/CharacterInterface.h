// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

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
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void StartSpeak();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void StopSpeak();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void PlaySound(class USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void StopSound();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void PlayMontage(class UAnimMontage* InMontage, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void PlayMontageByName(const FName InMontageName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void StopMontage(class UAnimMontage* InMontage, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void StopMontageByName(const FName InMontageName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void TeleportTo(FTransform InTransform, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void AIMoveTo(FTransform InTransform, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void StopAIMove(bool bMulticast = false);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void RotationTowards(FRotator InRotation, float InDuration = 1.f, bool bMulticast = false);

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	FName GetCharacterName() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	class UCharacterAnim* GetCharacterAnim() const;
};
