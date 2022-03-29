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
	static void RemoveCharacterFromListByName(const FString& InCharacterName);

	UFUNCTION(BlueprintPure, Category = "CharacterModuleBPLibrary")
	static TScriptInterface<ICharacterInterface> GetCharacterByName(const FString& InCharacterName);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void PlayCharacterSound(const FString& InCharacterName, class USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void StopCharacterSound(const FString& InCharacterName);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void PlayCharacterMontage(const FString& InCharacterName, class UAnimMontage* InMontage, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void PlayCharacterMontageByName(const FString& InCharacterName, const FName InMontageName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void StopCharacterMontage(const FString& InCharacterName, class UAnimMontage* InMontage, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void StopCharacterMontageByName(const FString& InCharacterName, const FName InMontageName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void TeleportCharacterTo(const FString& InCharacterName, FTransform InTransform, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void AIMoveCharacterTo(const FString& InCharacterName, FVector InLocation, float InStopDistance = 10.f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void StopCharacterAIMove(const FString& InCharacterName, bool bMulticast = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void RotationCharacterTowards(const FString& InCharacterName, FRotator InRotation, float InDuration = 1.f, bool bMulticast = false);
};
