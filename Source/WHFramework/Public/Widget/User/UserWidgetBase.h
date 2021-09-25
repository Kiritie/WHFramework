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
	void OnOpen(bool bInstant = false) override;
	
	UFUNCTION(BlueprintNativeEvent)
	void OnClose(bool bInstant = false) override;
	
	UFUNCTION(BlueprintNativeEvent)
	void OnRefresh() override;
	
	UFUNCTION(BlueprintNativeEvent)
	void OnDestroy() override;

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Open(bool bInstant = false) override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Close(bool bInstant = false) override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Toggle(bool bInstant = false) override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Refresh() override;

protected:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FName WidgetName;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	EWidgetType WidgetType;
		
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	EInputMode InputMode;
		
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	AActor* OwnerActor;

public:
	UFUNCTION(BlueprintPure)
	virtual bool IsOpened() const override;

	UFUNCTION(BlueprintPure)
	virtual FName GetWidgetName() const { return WidgetName; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetType GetWidgetType() const override { return WidgetType; }

	UFUNCTION(BlueprintPure)
	virtual EInputMode GetInputMode() const override { return InputMode; }

	UFUNCTION(BlueprintPure)
	virtual AActor* GetOwnerActor() const override { return OwnerActor; }
};
