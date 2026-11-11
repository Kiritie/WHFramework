#pragma once

#include "InputModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputModuleStatics.generated.h"

class APlayerController;
class UWorld;
class UInputBindingBase;
class UInputActionBase;
class UEnhancedInputLocalPlayerSubsystem;
class UInputUserSettingsBase;
class UInputComponentBase;

UCLASS()
class WHFRAMEWORK_API UInputModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="InputModuleStatics")
	static UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem(APlayerController* InPlayer);
	template<class T = UInputUserSettingsBase>
	static T* GetInputUserSettings(APlayerController* InPlayer) { return Cast<T>(GetInputUserSettings(InPlayer, T::StaticClass())); }
	UFUNCTION(BlueprintPure, meta=(DeterminesOutputType="InClass"), Category="InputModuleStatics")
	static UInputUserSettingsBase* GetInputUserSettings(APlayerController* InPlayer, TSubclassOf<UInputUserSettingsBase> InClass);
	template<class T = UInputComponentBase>
	static T* GetInputComponent(APlayerController* InPlayer) { return Cast<T>(GetInputComponent(InPlayer, T::StaticClass())); }
	UFUNCTION(BlueprintPure, meta=(DeterminesOutputType="InClass"), Category="InputModuleStatics")
	static UInputComponentBase* GetInputComponent(APlayerController* InPlayer, TSubclassOf<UInputComponentBase> InClass);
	template<class T>
	static T* GetInputBinding(APlayerController* InPlayer) { return Cast<T>(GetInputBinding(InPlayer, T::StaticClass())); }
	UFUNCTION(BlueprintPure, meta=(DeterminesOutputType="InClass"), Category="InputModuleStatics")
	static UInputBindingBase* GetInputBinding(APlayerController* InPlayer, TSubclassOf<UInputBindingBase> InClass);
	UFUNCTION(BlueprintPure, meta=(DeterminesOutputType="InClass"), Category="InputModuleStatics")
	static UInputBindingBase* GetInputBindingByName(APlayerController* InPlayer, FName InName, TSubclassOf<UInputBindingBase> InClass);
	UFUNCTION(BlueprintPure, Category="InputModuleStatics")
	static TArray<FEnhancedActionKeyMapping> GetAllActionKeyMappings(APlayerController* InPlayer);
	UFUNCTION(BlueprintPure, Category="InputModuleStatics")
	static TArray<FName> GetAllActionKeyMappingNames(APlayerController* InPlayer);
	UFUNCTION(BlueprintPure, Category="InputModuleStatics")
	static TArray<FPlayerKeyMapping> GetAllPlayerKeyMappings(APlayerController* InPlayer);
	UFUNCTION(BlueprintPure, meta=(AutoCreateRefTerm="InTag"), Category="InputModuleStatics")
	static bool IsPlayerMappedKeyByTag(APlayerController* InPlayer, const FGameplayTag& InTag, const FKey& InKey);
	UFUNCTION(BlueprintCallable, meta=(WorldContext="InWorld", AutoCreateRefTerm="InTag"), Category="InputModuleStatics")
	static const UInputActionBase* GetInputActionByTag(UWorld* InWorld, const FGameplayTag& InTag, bool bEnsured = true);
	UFUNCTION(BlueprintPure, meta=(AutoCreateRefTerm="InTag"), Category="InputModuleStatics")
	static bool IsInputActionActive(APlayerController* InPlayer, const FGameplayTag& InTag);
	UFUNCTION(BlueprintCallable, meta=(AutoCreateRefTerm="InContextTag"), Category="InputModuleStatics")
	static bool ActivateInputContext(APlayerController* InPlayer, FGameplayTag InContextTag);
	UFUNCTION(BlueprintCallable, meta=(AutoCreateRefTerm="InContextTag"), Category="InputModuleStatics")
	static bool DeactivateInputContext(APlayerController* InPlayer, FGameplayTag InContextTag);
	UFUNCTION(BlueprintPure, meta=(AutoCreateRefTerm="InContextTag"), Category="InputModuleStatics")
	static bool IsInputContextActive(APlayerController* InPlayer, FGameplayTag InContextTag);
	UFUNCTION(BlueprintCallable, meta=(AutoCreateRefTerm="InActionTag,OutFailureReason"), Category="InputModuleStatics")
	static bool MapPlayerKeyByTag(APlayerController* InPlayer, FGameplayTag InActionTag, FKey InNewKey, EPlayerMappableKeySlot InSlot, FGameplayTagContainer& OutFailureReason);
	UFUNCTION(BlueprintCallable, meta=(AutoCreateRefTerm="OutFailureReason"), Category="InputModuleStatics")
	static bool MapPlayerKeyByMappingName(APlayerController* InPlayer, FName InMappingName, FKey InNewKey, EPlayerMappableKeySlot InSlot, FGameplayTagContainer& OutFailureReason);
	UFUNCTION(BlueprintCallable, meta=(AutoCreateRefTerm="InActionTag"), Category="InputModuleStatics")
	static bool ResetPlayerKeyByTag(APlayerController* InPlayer, FGameplayTag InActionTag);
	UFUNCTION(BlueprintCallable, Category="InputModuleStatics")
	static bool ResetPlayerKeyByMappingName(APlayerController* InPlayer, FName InMappingName);
	UFUNCTION(BlueprintPure, meta=(AutoCreateRefTerm="InActionTag"), Category="InputModuleStatics")
	static TArray<FPlayerKeyMapping> GetPlayerKeyMappingsByTag(APlayerController* InPlayer, FGameplayTag InActionTag);
	UFUNCTION(BlueprintPure, meta=(AutoCreateRefTerm="InActionTag"), Category="InputModuleStatics")
	static TArray<FKey> GetKeysByActionTag(APlayerController* InPlayer, FGameplayTag InActionTag);
	UFUNCTION(BlueprintPure, Category="InputModuleStatics")
	static TArray<FPlayerKeyMapping> GetPlayerKeyMappingsByMappingName(APlayerController* InPlayer, FName InMappingName);
	UFUNCTION(BlueprintPure, meta=(AutoCreateRefTerm="InActionTag"), Category="InputModuleStatics")
	static FText GetPlayerKeyCodeByTag(APlayerController* InPlayer, FGameplayTag InActionTag);
	UFUNCTION(BlueprintPure, meta=(WorldContext="InWorld"), Category="InputModuleStatics")
	static TArray<FGameplayTag> GetAllMappableActions(UWorld* InWorld);
	UFUNCTION(BlueprintPure, meta=(WorldContext="InWorld"), Category="InputModuleStatics")
	static TArray<FInputMappableEntry> GetAllMappableEntries(UWorld* InWorld);
	UFUNCTION(BlueprintPure, meta=(WorldContext="InWorld", AutoCreateRefTerm="InActionTag"), Category="InputModuleStatics")
	static TArray<FInputMappableEntry> GetMappableEntriesByActionTag(UWorld* InWorld, FGameplayTag InActionTag);
	UFUNCTION(BlueprintPure, Category="InputModuleStatics")
	static ECommonInputType GetCurrentInputType(APlayerController* InPlayer);
	UFUNCTION(BlueprintPure, Category="InputModuleStatics")
	static bool IsUsingGamepad(APlayerController* InPlayer);
	UFUNCTION(BlueprintPure, Category="InputModuleStatics")
	static bool IsUsingMouseAndKeyboard(APlayerController* InPlayer);
	UFUNCTION(BlueprintPure, Category="InputModuleStatics")
	static bool IsUsingTouch(APlayerController* InPlayer);
	UFUNCTION(BlueprintPure, meta=(WorldContext="InWorld"), Category="InputModuleStatics")
	static EInputMode GetDefaultInputMode(const UObject* InWorld);
	UFUNCTION(BlueprintCallable, meta=(WorldContext="InWorld"), Category="InputModuleStatics")
	static void SetDefaultInputMode(const UObject* InWorld, EInputMode InInputMode);
	UFUNCTION(BlueprintPure, Category="InputModuleStatics")
	static EInputMode GetGlobalInputMode(APlayerController* InPlayer);
};
