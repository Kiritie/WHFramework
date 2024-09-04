// Fill out your copyright notice in the Description page of Project Settings.

#include "Main/Slate/SModuleDetailsWidget.h"

#include "SlateOptMacros.h"
#include "Main/MainModule.h"
#include "Main/Base/ModuleBase.h"
#include "Main/Slate/SModuleEditorWidget.h"
#include "Main/Slate/SModuleListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

const FName SModuleDetailsWidget::WidgetName = FName("ModuleDetailsWidget");

SModuleDetailsWidget::SModuleDetailsWidget()
{
	WidgetType = EEditorWidgetType::Child;
}

void SModuleDetailsWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
}

void SModuleDetailsWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ObjectsUseNameArea;
	DetailsViewArgs.bHideSelectionTip = false;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	ChildSlot
	[
		SNew(SBorder)
		.Padding(1.f)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SBox)
			.WidthOverride(420)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.FillHeight(1)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						DetailsView.ToSharedRef()
					]
				]
			]
		]
	];
}

void SModuleDetailsWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SModuleDetailsWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();

	if(GetParentWidgetN<SModuleEditorWidget>()->ListWidget->bDefaults)
	{
		DetailsView->SetObject(GetParentWidgetN<SModuleEditorWidget>()->MainModule);
	}
	else if(GetParentWidgetN<SModuleEditorWidget>()->ListWidget->SelectedModuleListItems.Num() > 0)
	{
		TArray<UObject*> Modules = TArray<UObject*>();
		for(auto Iter : GetParentWidgetN<SModuleEditorWidget>()->ListWidget->SelectedModuleListItems)
		{
			if(Iter->Module)
			{
				Modules.Add(Iter->Module);
			}
		}
		DetailsView->SetObjects(Modules);
	}
	else
	{
		DetailsView->SetObject(nullptr);
	}
}

void SModuleDetailsWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
