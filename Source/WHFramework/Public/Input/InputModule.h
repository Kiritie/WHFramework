// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "InputManager.h"
#include "Main/Base/ModuleBase.h"
#include "Input/InputModuleTypes.h"

#include "InputModule.generated.h"

class UInputMappingContext;
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "InputMappings|Context")
	TArray<UInputMappingContext*> ActionContexts;

	UPROPERTY(EditAnywhere, Category = "InputMappings|Key")
	TMap<FName, FInputKeyMapping> KeyMappings;

	UPROPERTY(EditAnywhere, Category = "InputMappings|Touch")
	TArray<FInputTouchMapping> TouchMappings;

protected:
	UFUNCTION(BlueprintNativeEvent)
	void OnBindAction(UEnhancedInputComponent* InInputComponent, UInputActionBase* InInputAction);

public:
	UFUNCTION(BlueprintPure)
	TArray<UInputMappingContext*>& GetActionContexts() { return ActionContexts; }

	TArray<FEnhancedActionKeyMapping*> GetActionMappingsByName(const FName InActionName);

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