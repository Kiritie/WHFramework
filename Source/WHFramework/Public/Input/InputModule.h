// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "InputManager.h"
#include "Main/Base/ModuleBase.h"
#include "Input/InputModuleTypes.h"

#include "InputModule.generated.h"

class UPlayerMappableInputConfig;
class UInputComponentBase;
class UPlayerMappableInputConfig;
class UInputActionBase;
class UEnhancedInputComponent;

UCLASS()
class WHFRAMEWORK_API AInputModule : public AModuleBase, public IInputManager
{
	GENERATED_BODY()
			
	GENERATED_MODULE(AInputModule)

public:	
	AInputModule();

	~AInputModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif
	
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnReset_Implementation() override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation(EPhase InPhase) override;

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual FSaveData* ToData() override;

	//////////////////////////////////////////////////////////////////////////
	// InputShortcuts
protected:
	UPROPERTY(EditAnywhere, Category = "InputShortcuts|Key")
	TMap<FName, FInputKeyShortcut> KeyShortcuts;

public:
	UFUNCTION(BlueprintPure)
	TMap<FName, FInputKeyShortcut>& GetKeyShortcuts() { return KeyShortcuts; }

	UFUNCTION(BlueprintPure)
	FInputKeyShortcut GetKeyShortcutByName(const FName InName) const;

	UFUNCTION(BlueprintCallable)
	void AddKeyShortcut(const FName InName, const FInputKeyShortcut& InKeyShortcut = FInputKeyShortcut());

	UFUNCTION(BlueprintCallable)
	void RemoveKeyShortcut(const FName InName);

	//////////////////////////////////////////////////////////////////////////
	// InputMappings
protected:
	UPROPERTY(EditAnywhere, Category = "InputMappings|Config")
	TArray<FInputConfigMapping> ConfigMapping;

	UPROPERTY(EditAnywhere, Category = "InputMappings|Key")
	TMap<FName, FInputKeyMapping> KeyMappings;

	UPROPERTY(EditAnywhere, Category = "InputMappings|Touch")
	TArray<FInputTouchMapping> TouchMappings;

protected:
	UFUNCTION(BlueprintNativeEvent)
	void OnBindAction(UInputComponentBase* InInputComponent, UPlayerMappableInputConfig* InInputConfig);

public:
	TArray<FInputConfigMapping>& GetConfigMapping() { return ConfigMapping; }

	UFUNCTION(BlueprintPure)
	TMap<FName, FInputKeyMapping>& GetKeyMappings() { return KeyMappings; }

	UFUNCTION(BlueprintPure)
	TArray<FInputTouchMapping>& GetTouchMappings() { return TouchMappings; }

	UFUNCTION(BlueprintCallable)
	void AddKeyMapping(const FName InName, const FInputKeyMapping& InKeyMapping);

	UFUNCTION(BlueprintCallable)
	void RemoveKeyMapping(const FName InName);

	UFUNCTION(BlueprintCallable)
	void AddTouchMapping(const FInputTouchMapping& InKeyMapping);

	UFUNCTION(BlueprintCallable)
	void ApplyKeyMappings();

	UFUNCTION(BlueprintCallable)
	void ApplyTouchMappings();

	//////////////////////////////////////////////////////////////////
	// Keybindings
public:
	// Sets the controller representation to use, a single platform might support multiple kinds of controllers.  For
	// example, Win64 games could be played with both an XBox or Playstation controller.
	UFUNCTION()
	void SetControllerPlatform(const FName InControllerPlatform);
	UFUNCTION()
	FName GetControllerPlatform() const;

	DECLARE_EVENT_OneParam(AInputModule, FInputConfigDelegate, const FLoadedInputConfigMapping& /*Config*/);

	/** Delegate called when a new input config has been registered */
	FInputConfigDelegate OnInputConfigRegistered;

	/** Delegate called when a registered input config has been activated */
	FInputConfigDelegate OnInputConfigActivated;
	
	/** Delegate called when a registered input config has been deactivate */
	FInputConfigDelegate OnInputConfigDeactivated;
	
	const UInputActionBase* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	/** Register the given input config with the settings to make it available to the player. */
	void RegisterInputConfig(ECommonInputType Type, const UPlayerMappableInputConfig* NewConfig, const bool bIsActive);
	
	/** Unregister the given input config. Returns the number of configs removed. */
	int32 UnregisterInputConfig(const UPlayerMappableInputConfig* ConfigToRemove);

	/** Get an input config with a certain name. If the config doesn't exist then nullptr will be returned. */
	UFUNCTION(BlueprintCallable)
	const UPlayerMappableInputConfig* GetInputConfigByName(FName ConfigName) const;

	/** Get all currently registered input configs */
	const TArray<FLoadedInputConfigMapping>& GetAllRegisteredInputConfigs() const { return RegisteredInputConfigs; }

	/**
	 * Get all registered input configs that match the input type.
	 * 
	 * @param Type		The type of config to get, ECommonInputType::Count will include all configs.
	 * @param OutArray	Array to be populated with the current registered input configs that match the type
	 */
	void GetRegisteredInputConfigsOfType(ECommonInputType Type, OUT TArray<FLoadedInputConfigMapping>& OutArray) const;

	/**
	 * Returns the display name of any actions with that key bound to it
	 * 
	 * @param InKey The key to check for current mappings of
	 * @param OutActionNames Array to store display names of actions of bound keys
	 */
	void GetAllMappingNamesFromKey(const FKey InKey, TArray<FName>& OutActionNames);

	void GetAllMappingByName(const FName InName, TArray<FEnhancedActionKeyMapping>& OutMappings);

	void GetAllMappingByDisplayName(const FText InDisplayName, TArray<FEnhancedActionKeyMapping>& OutMappings);

	/**
	 * Maps the given keyboard setting to the new key
	 * 
	 * @param MappingName	The name of the FPlayerMappableKeyOptions that you would like to change
	 * @param NewKey		The new key to bind this option to
	 * @param LocalPlayer   local player to reset the keybinding on
	 */
	void AddOrUpdateCustomKeyboardBindings(const FName MappingName, const FKey NewKey, ULocalPlayer* LocalPlayer);

	FKey GetCustomKeyboardBindings(const FName MappingName, ULocalPlayer* LocalPlayer);

	/**
	 * Resets keybinding to its default value in its input mapping context 
	 * 
	 * @param MappingName	The name of the FPlayerMappableKeyOptions that you would like to change
	 * @param LocalPlayer   local player to reset the keybinding on
	 */
	void ResetKeybindingToDefault(const FName MappingName, ULocalPlayer* LocalPlayer);

	/** Resets all keybindings to their default value in their input mapping context
	 * @param LocalPlayer   local player to reset the keybinding on
	 */
	void ResetKeybindingsToDefault(ULocalPlayer* LocalPlayer);

	const TMap<FName, FKey>& GetCustomPlayerInputConfig() const { return CustomKeyboardConfig; }

private:
	/**
	 * The name of the controller the player is using.  This is maps to the name of a UCommonInputBaseControllerData
	 * that is available on this current platform.  The gamepad data are registered per platform, you'll find them
	 * in <Platform>Game.ini files listed under +ControllerData=...
	 */
	UPROPERTY()
	FName ControllerPlatform;

	/** The name of the current input config that the user has selected. */
	UPROPERTY()
	FName InputConfigName = TEXT("Default");
	
	/**
	 * Array of currently registered input configs. This is populated by game feature plugins
	 * 
	 * @see UGameFeatureAction_AddInputConfig
	 */
	UPROPERTY(VisibleAnywhere)
	TArray<FLoadedInputConfigMapping> RegisteredInputConfigs;
	
	/** Array of custom key mappings that have been set by the player. Empty by default. */
	UPROPERTY(Config)
	TMap<FName, FKey> CustomKeyboardConfig;

	//////////////////////////////////////////////////////////////////////////
	/// CameraInputs
protected:
	UFUNCTION()
	virtual void TurnCamera(const FInputActionValue& InValue);

	UFUNCTION()
	virtual void LookUpCamera(const FInputActionValue& InValue);
	
	UFUNCTION()
	virtual void PanHCamera(const FInputActionValue& InValue);

	UFUNCTION()
	virtual void PanVCamera(const FInputActionValue& InValue);

	UFUNCTION()
	virtual void ZoomCamera(const FInputActionValue& InValue);

	//////////////////////////////////////////////////////////////////////////
	/// PlayerInputs
protected:
	UFUNCTION()
	virtual void TurnPlayer(const FInputActionValue& InValue);
	
	UFUNCTION()
	virtual void MoveHPlayer(const FInputActionValue& InValue);

	UFUNCTION()
	virtual void MoveVPlayer(const FInputActionValue& InValue);

	UFUNCTION()
	virtual void MoveForwardPlayer(const FInputActionValue& InValue);

	UFUNCTION()
	virtual void MoveRightPlayer(const FInputActionValue& InValue);

	UFUNCTION()
	virtual void MoveUpPlayer(const FInputActionValue& InValue);

	//////////////////////////////////////////////////////////////////////////
	/// TouchInputs
protected:
	UPROPERTY(EditAnywhere, Category = "InputSetup|Touch")
	float TouchInputRate;

protected:
	UFUNCTION()
	virtual void TouchPressed(ETouchIndex::Type InTouchIndex, FVector InLocation);

	UFUNCTION()
	virtual void TouchPressedImpl();

	UFUNCTION()
	virtual void TouchReleased(ETouchIndex::Type InTouchIndex, FVector InLocation);

	UFUNCTION()
	virtual void TouchReleasedImpl(ETouchIndex::Type InTouchIndex);

	UFUNCTION()
	virtual void TouchMoved(ETouchIndex::Type InTouchIndex, FVector InLocation);

	//////////////////////////////////////////////////////////////////////////
	/// InputStates
protected:
	UPROPERTY(VisibleAnywhere, Category = "InputStates|Touch")
	int32 TouchPressedCount;

	FVector2D TouchLocationPrevious;
	float TouchPinchValuePrevious;
	FTimerHandle TouchReleaseTimerHandle1;
	FTimerHandle TouchReleaseTimerHandle2;
	FTimerHandle TouchReleaseTimerHandle3;

public:
	UFUNCTION(BlueprintPure)
	int32 GetTouchPressedCount() const { return TouchPressedCount; }

	//////////////////////////////////////////////////////////////////////////
	// PlayerController
protected:
	UPROPERTY(Transient)
	AWHPlayerController* PlayerController;
	
protected:
	UFUNCTION(BlueprintPure)
	AWHPlayerController* GetPlayerController();

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	UFUNCTION(BlueprintCallable)
	virtual void UpdateInputMode();

protected:
	UPROPERTY(EditAnywhere, Category = "InputMode")
	EInputMode NativeInputMode;

	UPROPERTY(VisibleAnywhere, Category = "InputMode")
	EInputMode GlobalInputMode;
	
public:
	UFUNCTION(BlueprintCallable)
	virtual void SetGlobalInputMode(EInputMode InInputMode);

	UFUNCTION(BlueprintPure)
	EInputMode GetGlobalInputMode() const { return GlobalInputMode; }

	virtual EInputMode GetNativeInputMode() const override { return NativeInputMode; }
};