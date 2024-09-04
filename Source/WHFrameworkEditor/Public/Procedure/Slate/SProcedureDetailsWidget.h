// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Slate/Editor/Base/SEditorWidgetBase.h"

class FProcedureEditor;
class SProcedureListWidget;
class UProcedureModule;
/**
 * 
 */
class WHFRAMEWORKEDITOR_API SProcedureDetailsWidget : public SEditorWidgetBase
{
public:
	SProcedureDetailsWidget();
	
	SLATE_BEGIN_ARGS(SProcedureDetailsWidget) {}

		SLATE_ARGUMENT(TWeakPtr<FProcedureEditor>, ProcedureEditor)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	static const FName WidgetName;

public:
	virtual void OnCreate() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	TWeakPtr<FProcedureEditor> ProcedureEditor;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<IDetailsView> DetailsView;
};
