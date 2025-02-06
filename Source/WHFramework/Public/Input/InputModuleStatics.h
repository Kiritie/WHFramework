// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "InputModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputModuleStatics.generated.h"

class UInputManagerBase;
class UInputActionBase;
class UEnhancedInputLocalPlayerSubsystem;
class UInputUserSettingsBase;
class UInputComponentBase;
class UWHLocalPlayer;

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UInputModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	//////////////////////////////////////////////////////////////////////////
	// InputSubsystem
public:
	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem(int32 InPlayerIndex = 0);
	
	template<class T = UInputUserSettingsBase>
	static T* GetInputUserSettings(int32 InPlayerIndex = 0)
	{
		return Cast<T>(GetInputUserSettings(InPlayerIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "InputModuleStatics")
	static UInputUserSettingsBase* GetInputUserSettings(int32 InPlayerIndex = 0, TSubclassOf<UWHLocalPlayer> InClass = nullptr);
	
	template<class T = UInputComponentBase>
	static T* GetInputComponent(int32 InPlayerIndex = 0)
	{
		return Cast<T>(GetInputComponent(InPlayerIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "InputModuleStatics")
	static UInputComponentBase* GetInputComponent(int32 InPlayerIndex = 0, TSubclassOf<UInputComponentBase> InClass = nullptr);

public:
	template<class T>
	static T* GetInputManager(int32 InPlayerIndex = 0)
	{
		return Cast<T>(GetInputManager(T::StaticClass(), InPlayerIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "InputModuleStatics")
	static UInputManagerBase* GetInputManager(TSubclassOf<UInputManagerBase> InClass, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "InputModuleStatics")
	static UInputManagerBase* GetInputManagerByName(const FName InName, int32 InPlayerIndex = 0, TSubclassOf<UInputManagerBase> InClass = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// InputShortcuts
public:
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InTag"), Category = "InputModuleStatics")
	static void AddKeyShortcut(const FGameplayTag& InTag, const FInputKeyShortcut& InKeyShortcut = FInputKeyShortcut());

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InTag"), Category = "InputModuleStatics")
	static void RemoveKeyShortcut(const FGameplayTag& InTag);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"), Category = "InputModuleStatics")
	static FInputKeyShortcut GetKeyShortcut(const FGameplayTag& InTag);

	//////////////////////////////////////////////////////////////////////////
	// InputMappings
public:
	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InTag"), Category = "InputModuleStatics")
	static void AddKeyMapping(const FGameplayTag& InTag, const FInputKeyMapping& InKeyMapping);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InTag"), Category = "InputModuleStatics")
	static void RemoveKeyMapping(const FGameplayTag& InTag);

	UFUNCTION(BlueprintCallable, Category = "InputModuleStatics")
	static void AddTouchMapping(const FInputTouchMapping& InTouchMapping);

	UFUNCTION(BlueprintCallable, Category = "InputModuleStatics")
	static void AddPlayerKeyMapping(const FName InName, const FKey InKey, int32 InSlot = 0, int32 InPlayerIndex = 0);
	
	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static TArray<FPlayerKeyMapping> GetAllPlayerKeyMappings(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static TArray<FPlayerKeyMapping> GetPlayerKeyMappingsByName(const FName InName, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static FPlayerKeyMappingInfo GetPlayerKeyMappingInfoByName(const FName InName, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static bool IsPlayerMappedKeyByName(const FName InName, const FKey& InKey, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InTag"), Category = "InputModuleStatics")
	static const UInputActionBase* GetInputActionByTag(const FGameplayTag& InTag, bool bEnsured = true);

	//////////////////////////////////////////////////////////////////////////
	// InputStates
public:
	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static int32 GetTouchPressedCount();

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static EInputMode GetGlobalInputMode();

	UFUNCTION(BlueprintCallable, Category = "InputModuleStatics")
	static void UpdateGlobalInputMode();
};
