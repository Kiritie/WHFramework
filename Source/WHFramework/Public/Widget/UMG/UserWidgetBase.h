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
	void TickWidget_Implementation() override;

	bool IsTickAble_Implementation() const override { return bWidgetTickAble; }

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
	void Open(const TArray<FParameter>* InParams = nullptr, bool bInstant = false) override;
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, meta = (AutoCreateRefTerm = "InParams"))
	void Open(const TArray<FParameter>& InParams, bool bInstant = false) override;

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
	UPROPERTY(EditDefaultsOnly)
	FName WidgetName;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "WidgetType == EWidgetType::Child"))
	FName ParentName;
	
	UPROPERTY(EditDefaultsOnly)
	bool bWidgetTickAble;

	UPROPERTY(EditDefaultsOnly)
	int32 WidgetZOrder;

	UPROPERTY(EditDefaultsOnly)
	EWidgetType WidgetType;
			
	UPROPERTY(EditDefaultsOnly)
	EWidgetCreateType WidgetCreateType;
	
	UPROPERTY(EditDefaultsOnly)
	EWidgetOpenType WidgetOpenType;

	UPROPERTY(EditDefaultsOnly)
	EWidgetCloseType WidgetCloseType;

	UPROPERTY(EditDefaultsOnly)
	EWidgetRefreshType WidgetRefreshType;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "WidgetRefreshType == EWidgetRefreshType::Timer"))
	float WidgetRefreshTime;

	UPROPERTY(EditDefaultsOnly)
	EInputMode InputMode;

	UPROPERTY(Transient)
	EWidgetState WidgetState;

	UPROPERTY(Transient)
	AActor* OwnerActor;
	
	UPROPERTY(Transient)
	TScriptInterface<IWidgetInterface> LastWidget;
	
	UPROPERTY(Transient)
	TScriptInterface<IWidgetInterface> ParentWidget;
	
	UPROPERTY(Transient)
	TArray<TScriptInterface<IWidgetInterface>> ChildWidgets;

private:
	FTimerHandle WidgetRefreshTimerHandle;

public:
	UFUNCTION(BlueprintPure)
	virtual FName GetWidgetName() const override { return WidgetName; }

	UFUNCTION(BlueprintPure)
	virtual FName GetParentName() const override { return ParentName; }

	UFUNCTION(BlueprintPure)
	virtual int32 GetWidgetZOrder() const override { return WidgetZOrder; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetType GetWidgetType() const override { return WidgetType; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetCreateType GetWidgetCreateType() const override { return WidgetCreateType; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetOpenType GetWidgetOpenType() const override { return WidgetOpenType; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetCloseType GetWidgetCloseType() const override { return WidgetCloseType; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetRefreshType GetWidgetRefreshType() const override { return WidgetRefreshType; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetState GetWidgetState() const override
	{
		if(ParentWidget && ParentWidget->GetWidgetState() == EWidgetState::Closed)
		{
			return EWidgetState::Closed;
		}
		return WidgetState;
	}

	UFUNCTION(BlueprintPure)
	virtual EInputMode GetInputMode() const override { return InputMode; }

	UFUNCTION(BlueprintPure)
	virtual AActor* GetOwnerActor() const override { return OwnerActor; }

	UFUNCTION(BlueprintPure)
	virtual class UPanelWidget* GetRootPanelWidget() const override;

	UFUNCTION(BlueprintPure)
	virtual TScriptInterface<IWidgetInterface> GetLastWidget() const override { return LastWidget; }

	UFUNCTION(BlueprintCallable)
	virtual void SetLastWidget(TScriptInterface<IWidgetInterface> InLastWidget) override { LastWidget = InLastWidget; }

	UFUNCTION(BlueprintPure)
	virtual TScriptInterface<IWidgetInterface> GetParentWidget() const override { return ParentWidget; }

	UFUNCTION(BlueprintCallable)
	virtual void SetParentWidget(TScriptInterface<IWidgetInterface> InParentWidget) override { ParentWidget = InParentWidget; }

	UFUNCTION(BlueprintPure)
	virtual TArray<TScriptInterface<IWidgetInterface>>& GetChildWidgets() override { return ChildWidgets; }

	UFUNCTION(BlueprintCallable)
	virtual void SetChildWidgets(const TArray<TScriptInterface<IWidgetInterface>>& InChildWidgets) override { ChildWidgets = InChildWidgets; }
};
