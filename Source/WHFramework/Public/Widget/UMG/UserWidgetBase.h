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

protected:
	UPROPERTY(EditDefaultsOnly)
	bool bWidgetTickAble;

public:
	bool IsTickAble_Implementation() const override { return bWidgetTickAble; }

	void OnTick_Implementation(float DeltaSeconds) override;

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

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void AddChild(const TScriptInterface<IWidgetInterface>& InChildWidget) override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void RemoveChild(const TScriptInterface<IWidgetInterface>& InChildWidget) override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void RemoveAllChild(const TScriptInterface<IWidgetInterface>& InChildWidget) override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void RefreshAllChild() override;
	
protected:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void FinishOpen(bool bInstant) override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void FinishClose(bool bInstant) override;

protected:
	UPROPERTY(EditDefaultsOnly)
	EWidgetType WidgetType;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "WidgetType != EWidgetType::Child"))
	EWidgetCategory WidgetCategory;

	UPROPERTY(EditDefaultsOnly)
	FName WidgetName;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "WidgetType == EWidgetType::Child"))
	FName ParentName;
				
	UPROPERTY(EditDefaultsOnly)
	int32 WidgetZOrder;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "WidgetType == EWidgetType::Child"))
	FAnchors WidgetAnchors;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "WidgetType == EWidgetType::Child"))
	bool bWidgetAutoSize;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "WidgetType == EWidgetType::Child && bWidgetAutoSize == false"))
	FVector2D WidgetDrawSize;
	
	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "WidgetType == EWidgetType::Child && bWidgetAutoSize == false"))
	FMargin WidgetOffsets;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "WidgetType == EWidgetType::Child"))
	FVector2D WidgetAlignment;

	UPROPERTY(EditDefaultsOnly)
	EWidgetCreateType WidgetCreateType;
	
	UPROPERTY(EditDefaultsOnly)
	EWidgetOpenType WidgetOpenType;
	
	UPROPERTY(EditDefaultsOnly)
	EWidgetOpenFinishType WidgetOpenFinishType;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "WidgetOpenFinishType == EWidgetOpenFinishType::Delay"))
	float WidgetOpenFinishTime;

	UPROPERTY(EditDefaultsOnly)
	EWidgetCloseType WidgetCloseType;
	
	UPROPERTY(EditDefaultsOnly)
	EWidgetCloseFinishType WidgetCloseFinishType;

	UPROPERTY(EditDefaultsOnly, meta = (EditCondition = "WidgetCloseFinishType == EWidgetCloseFinishType::Delay"))
	float WidgetCloseFinishTime;

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
	FTimerHandle WidgetFinishOpenTimerHandle;
	FTimerHandle WidgetFinishCloseTimerHandle;
	FTimerHandle WidgetRefreshTimerHandle;

public:
	UFUNCTION(BlueprintPure)
	virtual EWidgetType GetWidgetType() const override { return WidgetType; }

	UFUNCTION(BlueprintPure)
	virtual EWidgetCategory GetWidgetCategory() const override
	{
		if(ParentWidget) return ParentWidget->GetWidgetCategory();
		return WidgetCategory;
	}

	UFUNCTION(BlueprintPure)
	virtual FName GetWidgetName() const override { return WidgetName; }

	UFUNCTION(BlueprintPure)
	virtual FName GetParentName() const override { return ParentName; }

	UFUNCTION(BlueprintPure)
	virtual int32 GetWidgetZOrder() const override { return WidgetZOrder; }

	UFUNCTION(BlueprintPure)
	virtual FAnchors GetWidgetAnchors() const override { return WidgetAnchors; }

	UFUNCTION(BlueprintPure)
	virtual bool IsWidgetAutoSize() const override { return bWidgetAutoSize; }

	UFUNCTION(BlueprintPure)
	virtual FVector2D GetWidgetDrawSize() const override { return WidgetDrawSize; }

	UFUNCTION(BlueprintPure)
	virtual FMargin GetWidgetOffsets() const override { return WidgetOffsets; }

	UFUNCTION(BlueprintPure)
	virtual FVector2D GetWidgetAlignment() const override { return WidgetAlignment; }

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
};
