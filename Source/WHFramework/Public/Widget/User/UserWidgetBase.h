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
	void OnInitialize(AActor* InOwner = nullptr) override;

	UFUNCTION(BlueprintNativeEvent)
	void OnOpen() override;
	
	UFUNCTION(BlueprintNativeEvent)
	void OnClose() override;

	UFUNCTION(BlueprintNativeEvent)
	void OnToggle() override;
	
	UFUNCTION(BlueprintNativeEvent)
	void OnRefresh() override;
	
	UFUNCTION(BlueprintNativeEvent)
	void OnDestroy() override;

protected:
	UFUNCTION(BlueprintNativeEvent)
	void OpenSelf() override;

	UFUNCTION(BlueprintNativeEvent)
	void CloseSelf() override;

protected:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	EWidgetType WidgetType;
		
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	EInputMode InputMode;
		
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	AActor* OwnerActor;

public:
	UFUNCTION(BlueprintPure)
	virtual EWidgetType GetWidgetType() const override { return WidgetType; }

	UFUNCTION(BlueprintPure)
	virtual EInputMode GetInputMode() const override { return InputMode; }

	UFUNCTION(BlueprintPure)
	virtual AActor* GetOwnerActor() const override { return OwnerActor; }
};
