// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CharacterModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CharacterModuleBPLibrary.generated.h"

class ACharacterBase;
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
	template<class T>
	static T* GetCurrentCharacter()
	{
		return Cast<T>(GetCurrentCharacter());
	}

	static ACharacterBase* GetCurrentCharacter();

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InCharacterClass"), Category = "CharacterModuleBPLibrary")
	static ACharacterBase* GetCurrentCharacter(TSubclassOf<ACharacterBase> InCharacterClass);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void SwitchCharacter(ACharacterBase* InCharacter, bool bResetCamera = true, bool bInstant = false);

	template<class T>
	static void SwitchCharacterByClass(bool bResetCamera = true, bool bInstant = false, TSubclassOf<ACharacterBase> InCharacterClass = T::StaticClass())
	{
		SwitchCharacterByClass(InCharacterClass, bResetCamera, bInstant);
	}

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void SwitchCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass, bool bResetCamera = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void SwitchCharacterByName(FName InCharacterName, bool bResetCamera = true, bool bInstant = false);

	template<class T>
	static bool HasCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass = T::StaticClass())
	{
		return HasCharacterByClass(InCharacterClass);
	}

	UFUNCTION(BlueprintPure, Category = "CharacterModuleBPLibrary")
	static bool HasCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass);

	UFUNCTION(BlueprintPure, Category = "CharacterModuleBPLibrary")
	static bool HasCharacterByName(FName InCharacterName);

	template<class T>
	static T* GetCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass = T::StaticClass())
	{
		return GetCharacterByClass(InCharacterClass);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InCharacterClass"), Category = "CharacterModuleBPLibrary")
	static ACharacterBase* GetCharacterByClass(TSubclassOf<ACharacterBase> InCharacterClass);

	UFUNCTION(BlueprintPure)
	static ACharacterBase* GetCharacterByName(FName InCharacterName);

public:
	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void AddCharacterToList(ACharacterBase* InCharacter);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleBPLibrary")
	static void RemoveCharacterFromList(ACharacterBase* InCharacter);
};
