// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "InputManager.h"
#include "Main/Base/ModuleBase.h"
#include "Input/InputModuleTypes.h"

#include "InputModule.generated.h"

class UInputComponentBase;
class UInputMappingContext;
class UInputActionBase;
class UEnhancedInputComponent;

UCLASS()
class WHFRAMEWORK_API UInputModule : public UModuleBase, public IInputManager
{
	GENERATED_BODY()
			
	GENERATED_MODULE(UInputModule)

public:	
	UInputModule();

	~UInputModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif
	
	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnReset() override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnBindAction")
	void K2_OnBindAction(UInputComponentBase* InInputComponent);
	UFUNCTION()
	virtual void OnBindAction(UInputComponentBase* InInputComponent);

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual void RefreshData();

	virtual FSaveData* ToData() override;

	virtual FSaveData* GetData() override;

protected:
	FInputModuleSaveData LocalSaveData;

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
	UPROPERTY(EditAnywhere, Category = "InputSteups|Context")
	TArray<FInputContextMapping> ContextMappings;

	UPROPERTY(VisibleAnywhere, Category = "InputSteups|Key")
	TMap<FName, FInputKeyMapping> KeyMappings;

	UPROPERTY(VisibleAnywhere, Category = "InputSteups|Touch")
	TArray<FInputTouchMapping> TouchMappings;
	
	UPROPERTY(VisibleAnywhere, Category = "InputSteups|Context")
	TMap<FName, FKey> CustomKeyMappings;

public:
	TArray<FInputContextMapping>& GetContextMappings() { return ContextMappings; }

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
	
	UFUNCTION(BlueprintPure, meta = (AutoCreateRefTerm = "InInputTag"))
	const UInputActionBase* FindInputActionForTag(const FGameplayTag& InInputTag, bool bEnsured = true) const;

	UFUNCTION(BlueprintPure)
	TArray<FEnhancedActionKeyMapping> GetAllActionKeyMappings(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure)
	TArray<FName> GetAllActionKeyMappingNames(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure)
	TArray<FPlayerKeyMapping> GetAllPlayerKeyMappings(int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintPure)
	TArray<FPlayerKeyMapping> GetPlayerKeyMappingsByName(const FName InName, int32 InPlayerIndex = 0);

	UFUNCTION(BlueprintCallable)
	void AddOrUpdateCustomKeyBindings(const FName InName, const FKey InKey, int32 InSlot = 0, int32 InPlayerIndex = 0);

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
	
	UFUNCTION()
	virtual void MoveForwardCamera(const FInputActionValue& InValue);

	UFUNCTION()
	virtual void MoveRightCamera(const FInputActionValue& InValue);

	UFUNCTION()
	virtual void MoveUpCamera(const FInputActionValue& InValue);

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

	UFUNCTION()
	virtual void JumpPlayer();

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
	virtual EInputMode GetGlobalInputMode() const { return GlobalInputMode; }

	virtual int32 GetNativeInputPriority() const override { return 0; }

	virtual EInputMode GetNativeInputMode() const override { return NativeInputMode; }
};
