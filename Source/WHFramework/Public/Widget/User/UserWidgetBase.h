// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "WidgetInterface.h"
#include "Blueprint/UserWidget.h"
#include "UserWidgetBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UUserWidgetBase : public UUserWidget, public IWidgetInterface
{
	GENERATED_BODY()

public:
	UUserWidgetBase(const FObjectInitializer& ObjectInitializer);

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnInitialize() override;

	UFUNCTION(BlueprintNativeEvent)
	void OnAttach() override;

	UFUNCTION(BlueprintNativeEvent)
	void OnOpen() override;
	
	UFUNCTION(BlueprintNativeEvent)
	void OnClose() override;
	
	UFUNCTION(BlueprintNativeEvent)
	void OnRefresh(float DeltaSeconds) override;

	UFUNCTION(BlueprintNativeEvent)
	void OnDetach() override;
	
	UFUNCTION(BlueprintNativeEvent)
	void OnDestroy() override;

protected:
	UFUNCTION(BlueprintNativeEvent)
	void OpenSelf() override;

	UFUNCTION(BlueprintNativeEvent)
	void CloseSelf() override;

protected:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	EWHWidgetType WidgetType;
		
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	EWHInputMode InputMode;

public:
	UFUNCTION(BlueprintPure)
	virtual EWHWidgetType GetWidgetType() const override { return WidgetType; }

	UFUNCTION(BlueprintPure)
	virtual EWHInputMode GetInputMode() const override { return InputMode; }
};
