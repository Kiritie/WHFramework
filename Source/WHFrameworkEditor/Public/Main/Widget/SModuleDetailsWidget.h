// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Main/Base/ModuleBase.h"
#include "Widget/Base/SEditorWidgetBase.h"

class SModuleListWidget;
class AMainModule;
/**
 * 
 */
class WHFRAMEWORKEDITOR_API SModuleDetailsWidget : public SEditorWidgetBase
{
public:
	SModuleDetailsWidget();
	
	SLATE_BEGIN_ARGS(SModuleDetailsWidget) {}

		SLATE_ARGUMENT(AMainModule*, MainModule)

		SLATE_ARGUMENT(TSharedPtr<SModuleListWidget>, ListWidget)

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
	TArray<TSharedPtr<FModuleListItem>> SelectedModuleListItems;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	AMainModule* MainModule;

	TSharedPtr<SModuleListWidget> ListWidget;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<IDetailsView> DetailsView;

private:
	void UpdateDetailsView();

	void OnSelectModuleListItem(TArray<TSharedPtr<FModuleListItem>> ModuleListItem);
};
