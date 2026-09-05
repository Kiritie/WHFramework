// Fill out your copyright notice in the Description page of Project Settings.

#include "Task/Slate/STaskDetailsWidget.h"

#include "SlateOptMacros.h"
#include "Task/Slate/STaskGraphWidget.h"
#include "Task/Base/TaskBase.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

const FName STaskDetailsWidget::WidgetName = FName("TaskDetailsWidget");

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
	const TSharedPtr<FTaskEditor> Editor = TaskEditor.Pin();
	if (!Editor) return;
	TArray<UObject*> Objects;
	if (Editor->GraphWidget)
		for (UTaskBase* Task : Editor->GraphWidget->GetSelectedTasks()) Objects.Add(Task);
	if (Objects.IsEmpty()) Objects.Add(Editor->GetEditingAsset());
	DetailsView->SetObjects(Objects);
}

void STaskDetailsWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
