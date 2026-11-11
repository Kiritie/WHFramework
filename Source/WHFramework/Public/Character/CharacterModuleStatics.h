// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CharacterModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CharacterModuleStatics.generated.h"

class ACharacterBase;
class APlayerController;
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
	static T* GetCurrentCharacter(APlayerController* InController)
	{
		return Cast<T>(GetCurrentCharacter(InController));
	}

	static ACharacterBase* GetCurrentCharacter(APlayerController* InController);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "CharacterModuleStatics")
	static ACharacterBase* GetCurrentCharacter(APlayerController* InController, TSubclassOf<ACharacterBase> InClass);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = "CharacterModuleStatics")
	static TArray<ACharacterBase*> GetAllCharacter(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleStatics")
	static void SwitchCharacter(APlayerController* InController, ACharacterBase* InCharacter, bool bResetCamera = true, bool bInstant = false);

	template<class T>
	static void SwitchCharacterByClass(APlayerController* InController, bool bResetCamera = true, bool bInstant = false, TSubclassOf<ACharacterBase> InClass = T::StaticClass())
	{
		SwitchCharacterByClass(InController, InClass, bResetCamera, bInstant);
	}

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleStatics")
	static void SwitchCharacterByClass(APlayerController* InController, TSubclassOf<ACharacterBase> InClass, bool bResetCamera = true, bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleStatics")
	static void SwitchCharacterByName(APlayerController* InController, FName InName, bool bResetCamera = true, bool bInstant = false);

	template<class T>
	static bool HasCharacterByClass(const UObject* WorldContextObject, TSubclassOf<ACharacterBase> InClass = T::StaticClass())
	{
		return HasCharacterByClass(WorldContextObject, InClass);
	}

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = "CharacterModuleStatics")
	static bool HasCharacterByClass(const UObject* WorldContextObject, TSubclassOf<ACharacterBase> InClass);

	UFUNCTION(BlueprintPure, Category = "CharacterModuleStatics")
	static bool HasCharacterByName(const UObject* WorldContextObject, FName InName);

	template<class T>
	static T* GetCharacterByClass(const UObject* WorldContextObject, TSubclassOf<ACharacterBase> InClass = T::StaticClass())
	{
		return Cast<T>(GetCharacterByClass(WorldContextObject, InClass));
	}

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "InClass"), Category = "CharacterModuleStatics")
	static ACharacterBase* GetCharacterByClass(const UObject* WorldContextObject, TSubclassOf<ACharacterBase> InClass);

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"), Category = "CharacterModuleStatics")
	static ACharacterBase* GetCharacterByName(const UObject* WorldContextObject, FName InName);

public:
	UFUNCTION(BlueprintCallable, Category = "CharacterModuleStatics")
	static void AddCharacterToList(ACharacterBase* InCharacter);

	UFUNCTION(BlueprintCallable, Category = "CharacterModuleStatics")
	static void RemoveCharacterFromList(ACharacterBase* InCharacter);
};
