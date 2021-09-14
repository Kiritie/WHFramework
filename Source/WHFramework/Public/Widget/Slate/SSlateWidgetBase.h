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
	virtual void OnInitialize(AActor* InOwner = nullptr) override;

	virtual void OnOpen(bool bInstant = true) override;
	
	virtual void OnClose(bool bInstant = true) override;

	virtual void OnToggle(bool bInstant = true) override;
	
	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

protected:
	virtual void OpenSelf(bool bInstant = true) override;

	virtual void CloseSelf(bool bInstant = true) override;

protected:
	EWidgetType WidgetType;
		
	EInputMode InputMode;

	AActor* OwnerActor;

public:
	virtual EWidgetType GetWidgetType() const override { return WidgetType; }

	virtual EInputMode GetInputMode() const override { return InputMode; }

	virtual AActor* GetOwnerActor() const override { return OwnerActor; }
};
