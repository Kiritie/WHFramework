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

	virtual void OnOpen(bool bInstant = false) override;
	
	virtual void OnClose(bool bInstant = false) override;
	
	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

public:
	virtual void Open(bool bInstant = false) override;

	virtual void Close(bool bInstant = false) override;

	virtual void Toggle(bool bInstant) override;

	virtual void Refresh() override;

protected:
	EWidgetType WidgetType;
		
	EInputMode InputMode;

	AActor* OwnerActor;

public:
	virtual bool IsOpened() const override;
	
	virtual EWidgetType GetWidgetType() const override { return WidgetType; }

	virtual EInputMode GetInputMode() const override { return InputMode; }

	virtual AActor* GetOwnerActor() const override { return OwnerActor; }
};
