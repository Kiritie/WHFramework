// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


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
	// InputShortcuts
public:
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static FInputKeyShortcut GetKeyShortcutByName(const FName InName);

	//////////////////////////////////////////////////////////////////////////
	/// WidgetInputs
public:
	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static FEventReply OnWidgetInputKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent);

	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static FEventReply OnWidgetInputMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent);

	//////////////////////////////////////////////////////////////////////////
	// InputStates
public:
	UFUNCTION(BlueprintPure, Category = "InputModuleBPLibrary")
	static int32 GetTouchPressedCount();

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
	// InputBinds
public:
	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void BindInputKey(const FKey Key, const EInputEvent KeyEvent, const FInputActionHandlerDynamicSignature& Delegate);

	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void BindInputAction(const FName ActionName, const EInputEvent KeyEvent, const FInputActionHandlerDynamicSignature& Delegate);

	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void BindInputAxis(const FName AxisName, const FInputAxisHandlerDynamicSignature& Delegate);

	UFUNCTION(BlueprintCallable, Category = "InputModuleBPLibrary")
	static void BindInputTouch(const EInputEvent KeyEvent, const FInputTouchHandlerDynamicSignature& Delegate);
};