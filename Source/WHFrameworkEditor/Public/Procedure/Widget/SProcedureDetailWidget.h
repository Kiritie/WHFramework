// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Procedure/Base/ProcedureBase.h"
#include "Widget/SEditorSlateWidgetBase.h"

class SProcedureListWidget;
class AProcedureModule;
/**
 * 
 */
class WHFRAMEWORKEDITOR_API SProcedureDetailWidget : public SEditorSlateWidgetBase
{
public:
	SLATE_BEGIN_ARGS(SProcedureDetailWidget) {}

		SLATE_ARGUMENT(AProcedureModule*, ProcedureModule)

		SLATE_ARGUMENT(TSharedPtr<SProcedureListWidget>, ListWidget)

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
	TArray<TSharedPtr<FProcedureListItem>> SelectedProcedureListItems;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	AProcedureModule* ProcedureModule;

	TSharedPtr<SProcedureListWidget> ListWidget;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<IDetailsView> DetailsView;

private:
	void UpdateDetailsView();

	void OnSelectProcedureListItem(TArray<TSharedPtr<FProcedureListItem>> ProcedureListItem);
};
