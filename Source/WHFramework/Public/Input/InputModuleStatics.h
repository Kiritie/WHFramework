// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "InputModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputModuleStatics.generated.h"

class UInputBindingBase;
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
	static T* GetInputBinding(int32 InPlayerIndex = 0)
	{
		return Cast<T>(GetInputBinding(T::StaticClass(), InPlayerIndex));
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "InputModuleStatics")
	static UInputBindingBase* GetInputBinding(TSubclassOf<UInputBindingBase> InClass, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"), Category = "InputModuleStatics")
	static UInputBindingBase* GetInputBindingByName(const FName InName, int32 InPlayerIndex = 0, TSubclassOf<UInputBindingBase> InClass = nullptr);

	//////////////////////////////////////////////////////////////////////////
	// InputMappings
public:
	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static TArray<FPlayerKeyMapping> GetAllPlayerKeyMappings(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"), Category = "InputModuleStatics")
	static bool IsPlayerMappedKeyByTag(const FGameplayTag& InTag, const FKey& InKey, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InTag"), Category = "InputModuleStatics")
	static const UInputActionBase* GetInputActionByTag(const FGameplayTag& InTag, bool bEnsured = true);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InTag"), Category = "InputModuleStatics")
	static bool IsInputActionActive(const FGameplayTag& InTag, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InContextTag"), Category = "InputModuleStatics")
	static bool ActivateInputContext(FGameplayTag InContextTag, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InContextTag"), Category = "InputModuleStatics")
	static bool DeactivateInputContext(FGameplayTag InContextTag, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InContextTag"), Category = "InputModuleStatics")
	static bool IsInputContextActive(FGameplayTag InContextTag, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InActionTag,OutFailureReason"), Category = "InputModuleStatics")
	static bool MapPlayerKeyByTag(FGameplayTag InActionTag, FKey InNewKey, EPlayerMappableKeySlot InSlot, FGameplayTagContainer& OutFailureReason, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "OutFailureReason"), Category = "InputModuleStatics")
	static bool MapPlayerKeyByMappingName(FName InMappingName, FKey InNewKey, EPlayerMappableKeySlot InSlot, FGameplayTagContainer& OutFailureReason, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, meta = (AutoCreateRefTerm = "InActionTag"), Category = "InputModuleStatics")
	static bool ResetPlayerKeyByTag(FGameplayTag InActionTag, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "InputModuleStatics")
	static bool ResetPlayerKeyByMappingName(FName InMappingName, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InActionTag"), Category = "InputModuleStatics")
	static TArray<FPlayerKeyMapping> GetPlayerKeyMappingsByTag(FGameplayTag InActionTag, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InActionTag"), Category = "InputModuleStatics")
	static TArray<FKey> GetKeysByActionTag(FGameplayTag InActionTag, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static TArray<FPlayerKeyMapping> GetPlayerKeyMappingsByMappingName(FName InMappingName, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InActionTag"), Category = "InputModuleStatics")
	static FText GetPlayerKeyCodeByTag(FGameplayTag InActionTag, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static TArray<FGameplayTag> GetAllMappableActions();

	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static TArray<FInputMappableEntry> GetAllMappableEntries();

	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InActionTag"), Category = "InputModuleStatics")
	static TArray<FInputMappableEntry> GetMappableEntriesByActionTag(FGameplayTag InActionTag);

	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static ECommonInputType GetCurrentInputType(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static bool IsUsingGamepad(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static bool IsUsingMouseAndKeyboard(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static bool IsUsingTouch(int32 InPlayerIndex = 0);

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static EInputMode GetDefaultInputMode();

	UFUNCTION(BlueprintCallable, Category = "InputModuleStatics")
	static void SetDefaultInputMode(EInputMode InInputMode);

	UFUNCTION(BlueprintPure, Category = "InputModuleStatics")
	static EInputMode GetGlobalInputMode();

	UFUNCTION(BlueprintCallable, Category = "InputModuleStatics")
	static void UpdateGlobalInputMode();
};
