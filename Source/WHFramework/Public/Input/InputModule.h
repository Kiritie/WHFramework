// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "InputManager.h"
#include "Main/Base/ModuleBase.h"
#include "Input/InputModuleTypes.h"
#include "Components/SlateWrapperTypes.h"

#include "InputModule.generated.h"

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

	//////////////////////////////////////////////////////////////////////////
	// InputShortcuts
protected:
	UPROPERTY(EditAnywhere, Category = "InputShortcuts|Key")
	TMap<FName, FInputKeyShortcut> KeyShortcuts;

public:
	UFUNCTION(BlueprintPure)
	FInputKeyShortcut GetKeyShortcutByName(const FName InName) const;

	//////////////////////////////////////////////////////////////////////////
	// InputMappings
protected:
	UPROPERTY(EditAnywhere, Category = "InputMappings|Key")
	TArray<FInputKeyMapping> KeyMappings;

	UPROPERTY(EditAnywhere, Category = "InputMappings|Action")
	TArray<FInputActionMapping> ActionMappings;

	UPROPERTY(EditAnywhere, Category = "InputMappings|Axis")
	TArray<FInputAxisMapping> AxisMappings;

	UPROPERTY(EditAnywhere, Category = "InputMappings|Touch")
	TArray<FInputTouchMapping> TouchMappings;

	//////////////////////////////////////////////////////////////////////////
	/// WidgetInputs
public:
	UFUNCTION(BlueprintNativeEvent)
	FEventReply OnWidgetInputKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent);

	UFUNCTION(BlueprintNativeEvent)
	FEventReply OnWidgetInputKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent);

	//////////////////////////////////////////////////////////////////////////
	/// CameraInputs
protected:
	UFUNCTION()
	virtual void TurnCamera(float InRate);

	UFUNCTION()
	virtual void LookUpCamera(float InRate);
	
	UFUNCTION()
	virtual void PanHCamera(float InRate);

	UFUNCTION()
	virtual void PanVCamera(float InRate);

	UFUNCTION()
	virtual void ZoomCamera(float InRate);

	//////////////////////////////////////////////////////////////////////////
	/// PlayerInputs
protected:
	UFUNCTION()
	virtual void TurnPlayer(float InValue);
	
	UFUNCTION()
	virtual void MoveHPlayer(float InValue);

	UFUNCTION()
	virtual void MoveVPlayer(float InValue);

	UFUNCTION()
	virtual void MoveForwardPlayer(float InValue);

	UFUNCTION()
	virtual void MoveRightPlayer(float InValue);

	UFUNCTION()
	virtual void MoveUpPlayer(float InValue);

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
protected:
	UPROPERTY(EditAnywhere, Category = "InputMode")
	EInputMode NativeInputMode;

	UPROPERTY(VisibleAnywhere, Category = "InputMode")
	EInputMode GlobalInputMode;
	
public:
	UFUNCTION(BlueprintCallable)
	virtual void UpdateInputMode();

	UFUNCTION(BlueprintCallable)
	virtual void SetGlobalInputMode(EInputMode InInputMode);

	UFUNCTION(BlueprintPure)
	EInputMode GetGlobalInputMode() const { return GlobalInputMode; }

	virtual EInputMode GetNativeInputMode() const override { return NativeInputMode; }
};
