// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/Widget/STaskDetailsWidget.h"

#include "SlateOptMacros.h"
#include "Task/TaskModule.h"
#include "Task/Base/TaskBase.h"
#include "Task/Widget/STaskListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

STaskDetailsWidget::STaskDetailsWidget()
{
	WidgetName = FName("TaskDetailsWidget");
	WidgetType = EEditorWidgetType::Child;
	TaskModule = nullptr;
}

void STaskDetailsWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	ListWidget = InArgs._ListWidget;
	TaskModule = InArgs._TaskModule;

	if(!TaskModule || !TaskModule->IsValidLowLevel() || !ListWidget) return;

	ListWidget->OnSelectTaskListItemsDelegate.BindRaw(this, &STaskDetailsWidget::OnSelectTaskListItem);

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

	UpdateDetailsView();
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
}

void STaskDetailsWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

void STaskDetailsWidget::UpdateDetailsView()
{
	if(SelectedTaskListItems.Num() > 0)
	{
		TArray<UObject*> Tasks = TArray<UObject*>();
		for(auto Iter : SelectedTaskListItems)
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
		DetailsView->SetObject(TaskModule);
	}
}

void STaskDetailsWidget::OnSelectTaskListItem(TArray<TSharedPtr<FTaskListItem>> TaskListItem)
{
	SelectedTaskListItems = TaskListItem;

	UpdateDetailsView();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
