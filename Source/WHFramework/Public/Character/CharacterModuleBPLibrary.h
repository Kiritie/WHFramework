// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CharacterModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CharacterModuleBPLibrary.generated.h"

class UCharacterHandleBase;
class ACharacterModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UCharacterModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void AddCharacterToList(TScriptInterface<class ICharacterInterface> InCharacter);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void RemoveCharacterFromList(TScriptInterface<ICharacterInterface> InCharacter);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void RemoveCharacterFromListByName(FName InCharacterName);

	UFUNCTION(BlueprintPure, Category = "CharacterModuleBPLibrary")
	static TScriptInterface<ICharacterInterface> GetCharacterByName(FName InCharacterName);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void PlayCharacterSound(FName InCharacterName, class USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void StopCharacterSound(FName InCharacterName);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void PlayCharacterMontage(FName InCharacterName, class UAnimMontage* InMontage, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void PlayCharacterMontageByName(FName InCharacterName, const FName InMontageName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void StopCharacterMontage(FName InCharacterName, class UAnimMontage* InMontage, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void StopCharacterMontageByName(FName InCharacterName, const FName InMontageName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void TeleportCharacterTo(FName InCharacterName, FTransform InTransform, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void AIMoveCharacterTo(FName InCharacterName, FVector InLocation, float InStopDistance = 10.f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void StopCharacterAIMove(FName InCharacterName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void RotationCharacterTowards(FName InCharacterName, FRotator InRotation, float InDuration = 1.f, bool bMulticast = false);
};
