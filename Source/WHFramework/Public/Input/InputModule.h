// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "InputManager.h"
#include "Main/Base/ModuleBase.h"
#include "Input/InputModuleTypes.h"

#include "InputModule.generated.h"

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
	UPROPERTY(EditAnywhere, Category = "InputSteups|Key")
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
	UPROPERTY(EditAnywhere, Category = "InputSteups|Config")
	TArray<FInputConfigMapping> ConfigMapping;

	UPROPERTY(VisibleAnywhere, Category = "InputSteups|Key")
	TMap<FName, FInputKeyMapping> KeyMappings;

	UPROPERTY(VisibleAnywhere, Category = "InputSteups|Touch")
	TArray<FInputTouchMapping> TouchMappings;
	
	UPROPERTY(VisibleAnywhere, Category = "InputSteups|Config")
	TArray<FLoadedInputConfigMapping> RegisteredConfigMapping;
	
	UPROPERTY(VisibleAnywhere, Category = "InputSteups|Config")
	TMap<FName, FKey> CustomKeyMappings;

	UPROPERTY(VisibleAnywhere, Category = "InputSteups|Config")
	FName ControllerPlatform;

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
	
	UFUNCTION(BlueprintPure)
	FName GetControllerPlatform() const { return ControllerPlatform; }

	UFUNCTION(BlueprintCallable)
	void SetControllerPlatform(const FName InControllerPlatform);
	
	UFUNCTION(BlueprintPure)
	const UInputActionBase* FindInputActionForTag(const FGameplayTag& InInputTag, const UPlayerMappableInputConfig* InConfig = nullptr, bool bLogNotFound = true) const;

	UFUNCTION(BlueprintCallable)
	void RegisterInputConfig(ECommonInputType InType, const UPlayerMappableInputConfig* InConfig, const bool bIsActive);
	
	UFUNCTION(BlueprintCallable)
	int32 UnregisterInputConfig(const UPlayerMappableInputConfig* InConfig);

	UFUNCTION(BlueprintPure)
	const UPlayerMappableInputConfig* GetInputConfigByName(FName InConfigName) const;

	UFUNCTION(BlueprintPure)
	TArray<FLoadedInputConfigMapping>& GetAllRegisteredConfigMapping() { return RegisteredConfigMapping; }

	UFUNCTION(BlueprintPure)
	TArray<FLoadedInputConfigMapping> GetRegisteredConfigMappingOfType(ECommonInputType InType);

	UFUNCTION(BlueprintPure)
	TArray<FEnhancedActionKeyMapping> GetAllPlayerMappableActionKeyMappings();

	UFUNCTION(BlueprintPure)
	TArray<FName> GetAllActionMappingNamesFromKey(const FKey InKey, int32 InPlayerID = 0);

	UFUNCTION(BlueprintPure)
	TArray<FEnhancedActionKeyMapping> GetAllActionMappingByName(const FName InName, int32 InPlayerID = 0);

	UFUNCTION(BlueprintPure)
	TArray<FEnhancedActionKeyMapping> GetAllActionMappingByDisplayName(const FText InDisplayName, int32 InPlayerID = 0);

	UFUNCTION(BlueprintCallable)
	void AddOrUpdateCustomKeyboardBindings(const FName InName, const FKey InKey, int32 InPlayerID = 0);

	UFUNCTION(BlueprintPure)
	const TMap<FName, FKey>& GetAllCustomKeyMappings() const { return CustomKeyMappings; }

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
	UPROPERTY(EditAnywhere, Category = "InputSteups|Touch")
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
