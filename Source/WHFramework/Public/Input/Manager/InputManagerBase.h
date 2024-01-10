// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "InputActionValue.h"
#include "Common/Base/WHObject.h"

#include "InputManagerBase.generated.h"

UCLASS()
class WHFRAMEWORK_API UInputManagerBase : public UWHObject
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this object's properties
	UInputManagerBase();

	//////////////////////////////////////////////////////////////////////////
	/// Defaults
public:
	/**
	* 当初始化
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize();
	UFUNCTION()
	virtual void OnInitialize();
	
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnBindAction")
	void K2_OnBindAction(UInputComponentBase* InInputComponent);
	UFUNCTION()
	virtual void OnBindAction(UInputComponentBase* InInputComponent);

protected:
	UPROPERTY(EditAnywhere, Category = "Input")
	FName InputManagerName;

public:
	UFUNCTION(BlueprintPure)
	FName GetInputManagerName() const { return InputManagerName; }
};
