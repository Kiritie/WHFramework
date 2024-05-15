// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"
#include "Input/InputManagerInterface.h"

#include "InputManagerBase.generated.h"

class UInputComponentBase;

UCLASS()
class WHFRAMEWORK_API UInputManagerBase : public UWHObject, public IInputManagerInterface
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
	/**
	* 当刷新
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh(float DeltaSeconds);
	UFUNCTION()
	virtual void OnRefresh(float DeltaSeconds);
	/**
	* 当绑定
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnBindAction")
	void K2_OnBindAction(UInputComponentBase* InInputComponent);
	UFUNCTION()
	virtual void OnBindAction(UInputComponentBase* InInputComponent);
	/**
	* 当销毁
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnTermination")
	void K2_OnTermination();
	UFUNCTION()
	virtual void OnTermination();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName InputManagerName;

public:
	UFUNCTION(BlueprintPure)
	FName GetInputManagerName() const { return InputManagerName; }

	UFUNCTION(BlueprintNativeEvent)
	int32 GetNativeInputPriority() const override;

	UFUNCTION(BlueprintNativeEvent)
	EInputMode GetNativeInputMode() const override;
};
