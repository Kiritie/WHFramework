// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Slate/Editor/Base/SEditorWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SModuleToolbarWidget : public SEditorWidgetBase
{
public:
	SModuleToolbarWidget();
	
	SLATE_BEGIN_ARGS(SModuleToolbarWidget) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	static FName WidgetName;

public:
	virtual void OnCreate() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

private:
	void OnPreviewToggled();

	void OnDefaultsToggled();

	void OnEditingToggled();
};
