// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Base/WHObject.h"
#include "InputBindingBase.generated.h"

class UInputComponentBase;

UCLASS(Abstract, BlueprintType, EditInlineNew)
class WHFRAMEWORK_API UInputBindingBase : public UWHObject
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this object's properties
	UInputBindingBase();

	//////////////////////////////////////////////////////////////////////////
	/// Defaults
public:
	/**
	* 当初始化
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize();
	UFUNCTION()
	virtual void OnInitialize(int32 InPlayerIndex);
	/**
	* 当重置
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnReset")
	void K2_OnReset();
	UFUNCTION()
	virtual void OnReset();
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
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnBindInput")
	void K2_OnBindInput(UInputComponentBase* InInputComponent);
	UFUNCTION()
	virtual void OnBindInput(UInputComponentBase* InInputComponent);
	UFUNCTION()
	virtual void OnUnbindInput();
	/**
	* 当销毁
	*/
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnTermination")
	void K2_OnTermination();
	UFUNCTION()
	virtual void OnTermination();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName InputBindingName;

	UPROPERTY(Transient, BlueprintReadOnly)
	int32 LocalPlayerIndex;
 
public:
	UFUNCTION(BlueprintPure)
	FName GetInputBindingName() const { return InputBindingName; }

	UFUNCTION(BlueprintPure)
	int32 GetLocalPlayerIndex() const { return LocalPlayerIndex; }

	void AddBindingHandle(uint32 InHandle);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UInputComponentBase> BoundInputComponent;

	TArray<uint32> BindingHandles;
};
