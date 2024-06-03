// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Slate/Editor/Base/SEditorWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SModuleStatusWidget : public SEditorWidgetBase
{
public:
	SModuleStatusWidget();
	
	SLATE_BEGIN_ARGS(SModuleStatusWidget) {}
	
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	static const FName WidgetName;

public:
	virtual void OnCreate() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;
};
