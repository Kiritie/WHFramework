// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "InputModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputModuleStatics.generated.h"

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

	//////////////////////////////////////////////////////////////////////////
	// InputShortcuts
public:
	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static FInputKeyShortcut GetKeyShortcutByName(const FName InName);

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
	static void SetGlobalInputMode(EInputMode InInputMode);

	UFUNCTION(BlueprintCallable, Category = "InputModuleStatics")
	static void UpdateGlobalInputMode();
};