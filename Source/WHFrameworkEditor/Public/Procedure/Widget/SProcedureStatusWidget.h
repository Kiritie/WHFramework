// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/Base/SEditorWidgetBase.h"

class FProcedureEditor;

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SProcedureStatusWidget : public SEditorWidgetBase
{
public:
	SProcedureStatusWidget();
	
	SLATE_BEGIN_ARGS(SProcedureStatusWidget) {}

		SLATE_ARGUMENT(TWeakPtr<FProcedureEditor>, ProcedureEditor)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	virtual void OnCreate() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	TWeakPtr<FProcedureEditor> ProcedureEditor;
};
