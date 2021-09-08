// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "WidgetInterface.h"
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
	virtual void OnInitialize() override;

	virtual void OnAttach() override;
	
	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnOpen() override;
	
	virtual void OnClose() override;

	virtual void OnDetach() override;
	
	virtual void OnDestroy() override;

protected:
	virtual void OpenSelf() override;

	virtual void CloseSelf() override;

protected:
	EWHWidgetType WidgetType;
		
	EWHInputMode InputMode;

public:
	virtual EWHWidgetType GetWidgetType() const override { return WidgetType; }

	virtual EWHInputMode GetInputMode() const override { return InputMode; }
};
