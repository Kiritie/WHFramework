// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "InputModuleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputModuleBPLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UInputModuleBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	//////////////////////////////////////////////////////////////////////////
	// InputMode
public:
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static EInputMode GetGlobalInputMode();

	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void SetGlobalInputMode(EInputMode InInputMode);

	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void UpdateGlobalInputMode();

	//////////////////////////////////////////////////////////////////////////
	// InputBind
public:
	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void BindInputKey(const FKey Key, const EInputEvent KeyEvent, const FInputActionHandlerDynamicSignature& Delegate);

	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void BindInputAction(const FName ActionName, const EInputEvent KeyEvent, const FInputActionHandlerDynamicSignature& Delegate);

	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void BindInputAxis(const FName AxisName, const FInputAxisHandlerDynamicSignature& Delegate);

	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void BindInputTouch(const EInputEvent KeyEvent, const FInputTouchHandlerDynamicSignature& Delegate);

	//////////////////////////////////////////////////////////////////////////
	// InputEvent
public:
	/** Returns true if the given key/button is pressed on the input of the controller (if present) */
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static bool IsInputKeyDown(FKey Key);

	/** Returns true if the given key/button was up last frame and down this frame. */
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static bool WasInputKeyJustPressed(FKey Key);

	/** Returns true if the given key/button was down last frame and up this frame. */
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static bool WasInputKeyJustReleased(FKey Key);

	/** Returns the analog value for the given key/button.  If analog isn't supported, returns 1 for down and 0 for up. */
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static float GetInputAnalogKeyState(FKey Key);

	/** Returns the vector value for the given key/button. */
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static FVector GetInputVectorKeyState(FKey Key);

	/** Retrieves the X and Y screen coordinates of the specified touch key. Returns false if the touch index is not down */
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static void GetInputTouchState(ETouchIndex::Type FingerIndex, float& LocationX, float& LocationY, bool& bIsCurrentlyPressed);

	/** Retrieves the current motion state of the player's input device */
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static void GetInputMotionState(FVector& Tilt, FVector& RotationRate, FVector& Gravity, FVector& Acceleration);

	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static bool GetShowMouseCursor();

	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void SetShowMouseCursor(bool bShow);

	/** Retrieves the X and Y screen coordinates of the mouse cursor. Returns false if there is no associated mouse device */
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static bool GetMousePosition(float& LocationX, float& LocationY);

	/** Positions the mouse cursor in screen space, in pixels. */
	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void SetMouseLocation( const int X, const int Y );

	/** Returns how long the given key/button has been down.  Returns 0 if it's up or it just went down this frame. */
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static float GetInputKeyTimeDown(FKey Key);

	/** Retrieves how far the mouse moved this frame. */
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static void GetInputMouseDelta(float& DeltaX, float& DeltaY);

	/** Retrieves the X and Y displacement of the given analog stick. */
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static void GetInputAnalogStickState(EControllerAnalogStick::Type WhichStick, float& StickX, float& StickY);

	/** Activates a new touch interface for this player controller */
	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void ActivateTouchInterface(class UTouchInterface* NewTouchInterface);

	/** Set the virtual joystick visibility. */
	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void SetVirtualJoystickVisibility(bool bVisible);
};