// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widget/WidgetInterface.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORK_API SSlateWidgetBase : public SCompoundWidget, public IWidgetInterface
{
public:
	SLATE_BEGIN_ARGS(SSlateWidgetBase)
	{}
	SLATE_END_ARGS()

	SSlateWidgetBase();

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

public:
	virtual void OnCreate() override;
	
	virtual void OnInitialize(AActor* InOwner = nullptr) override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant = false) override;
	
	virtual void OnClose(bool bInstant = false) override;

	virtual void OnReset() override;
	
	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

public:
	virtual void Open(const TArray<FParameter>* InParams = nullptr, bool bInstant = false) override;
	
	virtual void Open(const TArray<FParameter>& InParams, bool bInstant = false) override;
	
	virtual void Close(bool bInstant = false) override;

	virtual void Toggle(bool bInstant) override;

	virtual void Reset() override;

	virtual void Refresh() override;

	virtual void Destroy() override;

protected:
	virtual void FinishOpen(bool bInstant) override;

	virtual void FinishClose(bool bInstant) override;

protected:
	FName WidgetName;

	FName ParentName;
	
	int32 WidgetZOrder;

	EWidgetType WidgetType;
								
	EWidgetCreateType WidgetCreateType;
				
	EWidgetOpenType WidgetOpenType;

	EWidgetCloseType WidgetCloseType;
	
	EWidgetRefreshType WidgetRefreshType;

	EInputMode InputMode;

	EWidgetState WidgetState;

	AActor* OwnerActor;
	
	TScriptInterface<IWidgetInterface> LastWidget;
	
	TScriptInterface<IWidgetInterface> ParentWidget;
	
	TArray<TScriptInterface<IWidgetInterface>> ChildWidgets;

public:
	virtual FName GetWidgetName() const override { return WidgetName; }

	virtual FName GetParentName() const override { return ParentName; }

	virtual int32 GetWidgetZOrder() const override { return WidgetZOrder; }
	
	virtual EWidgetType GetWidgetType() const override { return WidgetType; }
	
	virtual EWidgetState GetWidgetState() const override
	{
		if(ParentWidget && ParentWidget->GetWidgetState() == EWidgetState::Closed)
		{
			return EWidgetState::Closed;
		}
		return WidgetState;
	}

	virtual EWidgetCreateType GetWidgetCreateType() const override { return WidgetCreateType; }

	virtual EWidgetOpenType GetWidgetOpenType() const override { return WidgetOpenType; }

	virtual EWidgetCloseType GetWidgetCloseType() const override { return WidgetCloseType; }
	
	virtual EWidgetRefreshType GetWidgetRefreshType() const override { return WidgetRefreshType; }

	virtual EInputMode GetInputMode() const override { return InputMode; }

	virtual AActor* GetOwnerActor() const override { return OwnerActor; }

	virtual TScriptInterface<IWidgetInterface> GetLastWidget() const override { return LastWidget; }

	virtual void SetLastWidget(TScriptInterface<IWidgetInterface> InLastWidget) override { LastWidget = InLastWidget; }

	virtual TScriptInterface<IWidgetInterface> GetParentWidget() const override { return ParentWidget; }

	virtual void SetParentWidget(TScriptInterface<IWidgetInterface> InParentWidget) override { ParentWidget = InParentWidget; }

	virtual TArray<TScriptInterface<IWidgetInterface>>& GetChildWidgets() override { return ChildWidgets; }

	virtual void SetChildWidgets(const TArray<TScriptInterface<IWidgetInterface>>& InChildWidgets) override { ChildWidgets = InChildWidgets; }
};
