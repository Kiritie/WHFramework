// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CharacterModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CharacterModuleStatics.generated.h"

class ACharacterBase;
class UCharacterHandleBase;
class UCharacterModule;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UCharacterModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	template<class T>
	static T* GetCurrentCharacter()
	{
		return Cast<T>(GetCurrentCharacter());
	}

	static ACharacterBase* GetCurrentCharacter();

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CharacterModuleStatics")
	static ACharacterBase* GetCurrentCharacter(TSubclassOf<ACharacterBase> InClass);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleStatics")
	static void SwitchCharacter(ACharacterBase* InCharacter, bool bResetCamera = true, bool bInstant = false);

	template<class T>
	static void SwitchCharacterByClass(bool bResetCamera = true, bool bInstant = false, TSubclassOf<ACharacterBase> InClass = T::StaticClass())
	{
		SwitchCharacterByClass(InClass, bResetCamera, bInstant);
	}

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleStatics")
	static void SwitchCharacterByClass(TSubclassOf<ACharacterBase> InClass, bool bResetCamera = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleStatics")
	static void SwitchCharacterByName(FName InName, bool bResetCamera = true, bool bInstant = false);

	template<class T>
	static bool HasCharacterByClass(TSubclassOf<ACharacterBase> InClass = T::StaticClass())
	{
		return HasCharacterByClass(InClass);
	}

	UFUNCTION(BlueprintPure, Category = "CharacterModuleStatics")
	static bool HasCharacterByClass(TSubclassOf<ACharacterBase> InClass);

	UFUNCTION(BlueprintPure, Category = "CharacterModuleStatics")
	static bool HasCharacterByName(FName InName);

	template<class T>
	static T* GetCharacterByClass(TSubclassOf<ACharacterBase> InClass = T::StaticClass())
	{
		return GetCharacterByClass(InClass);
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CharacterModuleStatics")
	static ACharacterBase* GetCharacterByClass(TSubclassOf<ACharacterBase> InClass);

	UFUNCTION(BlueprintPure)
	static ACharacterBase* GetCharacterByName(FName InName);

public:
	UFUNCTION(BlueprintCallable, Category = "CharacterModuleStatics")
	static void AddCharacterToList(ACharacterBase* InCharacter);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleStatics")
	static void RemoveCharacterFromList(ACharacterBase* InCharacter);
};
