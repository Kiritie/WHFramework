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

	virtual void OnReset() override;

	virtual void OnRefresh(float DeltaSeconds) override;
	
	virtual void OnBindAction(UInputComponentBase* InInputComponent) override;

	virtual void OnTermination() override;

	//////////////////////////////////////////////////////////////////////////
	/// System
protected:
	UFUNCTION(BlueprintNativeEvent)
	void SystemOperation();

	//////////////////////////////////////////////////////////////////////////
	/// Camera
protected:
	UFUNCTION(BlueprintNativeEvent)
	void OnTurnCamera(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void OnLookUpCamera(const FInputActionValue& InValue);
	
	UFUNCTION(BlueprintNativeEvent)
	void OnPanHCamera(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void OnPanVCamera(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void OnZoomCamera(const FInputActionValue& InValue);
	
	UFUNCTION(BlueprintNativeEvent)
	void OnMoveForwardCamera(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void OnMoveRightCamera(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void OnMoveUpCamera(const FInputActionValue& InValue);

	//////////////////////////////////////////////////////////////////////////
	/// Player
protected:
	UFUNCTION(BlueprintNativeEvent)
	void OnTurnPlayer(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void OnMoveForwardPlayer(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void OnActionForwardPlayer(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void OnMoveRightPlayer(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void OnActionRightPlayer(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void OnMoveUpPlayer(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void OnActionUpPlayer(const FInputActionValue& InValue);

	UFUNCTION(BlueprintNativeEvent)
	void OnJumpPlayer();

	UFUNCTION(BlueprintNativeEvent)
	void OnPrimaryPressed();

	UFUNCTION(BlueprintNativeEvent)
	void OnPrimaryRepeated();

	UFUNCTION(BlueprintNativeEvent)
	void OnPrimaryReleased();

	UFUNCTION(BlueprintNativeEvent)
	void OnSecondaryPressed();

	UFUNCTION(BlueprintNativeEvent)
	void OnSecondaryRepeated();

	UFUNCTION(BlueprintNativeEvent)
	void OnSecondaryReleased();

	UFUNCTION(BlueprintNativeEvent)
	void OnThirdPressed();

	UFUNCTION(BlueprintNativeEvent)
	void OnThirdRepeated();

	UFUNCTION(BlueprintNativeEvent)
	void OnThirdReleased();
};
