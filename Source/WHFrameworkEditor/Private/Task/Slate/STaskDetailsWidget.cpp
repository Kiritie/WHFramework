// Fill out your copyright notice in the Description page of Project Settings.

#include "Task/Slate/STaskDetailsWidget.h"

#include "SlateOptMacros.h"
#include "Task/Slate/STaskListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

FName STaskDetailsWidget::WidgetName = FName("TaskDetailsWidget");

STaskDetailsWidget::STaskDetailsWidget()
{
	WidgetType = EEditorWidgetType::Child;
}

void STaskDetailsWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
	
	TaskEditor = InArgs._TaskEditor;

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

void STaskDetailsWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void STaskDetailsWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void STaskDetailsWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();

	if(TaskEditor.Pin()->ListWidget->bDefaults)
	{
		DetailsView->SetObject(TaskEditor.Pin()->GetEditingAsset());
	}
	else if(TaskEditor.Pin()->ListWidget->SelectedTaskListItems.Num() > 0)
	{
		TArray<UObject*> Tasks = TArray<UObject*>();
		for(auto Iter : TaskEditor.Pin()->ListWidget->SelectedTaskListItems)
		{
			if(Iter->Task)
			{
				Tasks.Add(Iter->Task);
			}
		}
		DetailsView->SetObjects(Tasks);
	}
	else
	{
		DetailsView->SetObject(nullptr);
	}
}

void STaskDetailsWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
