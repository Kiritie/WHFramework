// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "InputActionValue.h"
#include "InputManagerBase.h"

#include "DefaultInputManagerBase.generated.h"

UCLASS()
class WHFRAMEWORK_API UDefaultInputManagerBase : public UInputManagerBase
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this object's properties
	UDefaultInputManagerBase();

	//////////////////////////////////////////////////////////////////////////
	/// Defaults
public:
	virtual void OnInitialize() override;
	
	virtual void OnBindAction(UInputComponentBase* InInputComponent) override;

	//////////////////////////////////////////////////////////////////////////
	/// System
protected:
	UFUNCTION(BlueprintNativeEvent)
	void SystemOperation();

	//////////////////////////////////////////////////////////////////////////
	/// Camera
protected:
	UFUNCTION(BlueprintNativeEvent)
	void TurnCamera(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void LookUpCamera(const FInputActionValue& InValue);
	
	UFUNCTION(BlueprintNativeEvent)
	void PanHCamera(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void PanVCamera(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void ZoomCamera(const FInputActionValue& InValue);
	
	UFUNCTION(BlueprintNativeEvent)
	void MoveForwardCamera(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void MoveRightCamera(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void MoveUpCamera(const FInputActionValue& InValue);

	//////////////////////////////////////////////////////////////////////////
	/// Player
protected:
	UFUNCTION(BlueprintNativeEvent)
	void TurnPlayer(const FInputActionValue& InValue);
	
	UFUNCTION(BlueprintNativeEvent)
	void MoveHPlayer(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void MoveVPlayer(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void MoveForwardPlayer(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void MoveRightPlayer(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void MoveUpPlayer(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void JumpPlayer();
};
