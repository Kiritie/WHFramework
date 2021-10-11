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
	static void RemoveCharacterFromListByName(const FName InCharacterName);

	UFUNCTION(BlueprintPure, Category = "CharacterModuleBPLibrary")
	static TScriptInterface<ICharacterInterface> GetCharacterByName(const FName InCharacterName);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void PlayCharacterSound(const FName InCharacterName, class USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void StopCharacterSound(const FName InCharacterName);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void PlayCharacterMontage(const FName InCharacterName, class UAnimMontage* InMontage, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void PlayCharacterMontageByName(const FName InCharacterName, const FName InMontageName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void StopCharacterMontage(const FName InCharacterName, class UAnimMontage* InMontage, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void StopCharacterMontageByName(const FName InCharacterName, const FName InMontageName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void TeleportCharacterTo(const FName InCharacterName, FTransform InTransform, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void AIMoveCharacterTo(const FName InCharacterName, FTransform InTransform, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void StopCharacterAIMove(const FName InCharacterName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void RotationCharacterTowards(const FName InCharacterName, FRotator InRotation, float InDuration = 1.f, bool bMulticast = false);
};
