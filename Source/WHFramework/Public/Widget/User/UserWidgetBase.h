// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Widget/WidgetInterface.h"

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
	void OnCreate() override;

	UFUNCTION(BlueprintNativeEvent)
	void OnInitialize(AActor* InOwner = nullptr) override;

	UFUNCTION(BlueprintNativeEvent)
	void OnOpen(const TArray<FParameter>& InParams, bool bInstant = false) override;
	
	UFUNCTION(BlueprintNativeEvent)
	void OnClose(bool bInstant = false) override;

	UFUNCTION(BlueprintNativeEvent)
	void OnReset() override;
	
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
	void Reset() override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Refresh() override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Destroy() override;
	
protected:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void FinishOpen(bool bInstant) override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void FinishClose(bool bInstant) override;

protected:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FName WidgetName;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	EWidgetType WidgetType;
		
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	EWidgetState WidgetState;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	EInputMode InputMode;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	AActor* OwnerActor;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TScriptInterface<IWidgetInterface> LastWidget;

public:
	UFUNCTION(BlueprintPure)
	virtual FName GetWidgetName() const { return WidgetName; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetType GetWidgetType() const override { return WidgetType; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetState GetWidgetState() const override { return WidgetState; }

	UFUNCTION(BlueprintPure)
	virtual EInputMode GetInputMode() const override { return InputMode; }

	UFUNCTION(BlueprintPure)
	virtual AActor* GetOwnerActor() const override { return OwnerActor; }

	UFUNCTION(BlueprintPure)
	virtual TScriptInterface<IWidgetInterface> GetLastWidget() const override { return LastWidget; }

	UFUNCTION(BlueprintCallable)
	virtual void SetLastWidget(TScriptInterface<IWidgetInterface> InLastWidget) override { LastWidget = InLastWidget; }
};
