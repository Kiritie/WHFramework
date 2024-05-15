// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Slate/Editor/Base/SEditorWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SModuleDetailsWidget : public SEditorWidgetBase
{
public:
	SModuleDetailsWidget();
	
	SLATE_BEGIN_ARGS(SModuleDetailsWidget) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	static FName WidgetName;

public:
	virtual void OnCreate() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<IDetailsView> DetailsView;
};
