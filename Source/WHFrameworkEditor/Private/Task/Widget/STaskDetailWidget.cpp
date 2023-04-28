// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/Widget/STaskDetailWidget.h"

#include "ISinglePropertyView.h"
#include "SlateOptMacros.h"
#include "Task/TaskModule.h"
#include "Task/Base/TaskBase.h"
#include "Task/Widget/STaskListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

STaskDetailWidget::STaskDetailWidget()
{
	WidgetName = FName("TaskDetailWidget");
	WidgetType = EEditorWidgetType::Child;
	TaskModule = nullptr;
}

void STaskDetailWidget::Construct(const FArguments& InArgs)
{
	SEditorSlateWidgetBase::Construct(SEditorSlateWidgetBase::FArguments());

	ListWidget = InArgs._ListWidget;
	TaskModule = InArgs._TaskModule;

	if(!TaskModule || !TaskModule->IsValidLowLevel() || !ListWidget) return;

	ListWidget->OnSelectTaskListItemsDelegate.BindRaw(this, &STaskDetailWidget::OnSelectTaskListItem);

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
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Center)
					.AutoWidth()
					.Padding(5.f, 2.f, 5.f, 2.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Detail Panel")))
					]
				]

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

void STaskDetailWidget::OnCreate()
{
	SEditorSlateWidgetBase::OnCreate();
}

void STaskDetailWidget::OnReset()
{
	SEditorSlateWidgetBase::OnReset();
}

void STaskDetailWidget::OnRefresh()
{
	SEditorSlateWidgetBase::OnRefresh();
}

void STaskDetailWidget::OnDestroy()
{
	SEditorSlateWidgetBase::OnDestroy();
}

void STaskDetailWidget::UpdateDetailsView()
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

void STaskDetailWidget::OnSelectTaskListItem(TArray<TSharedPtr<FTaskListItem>> TaskListItem)
{
	SelectedTaskListItems = TaskListItem;

	UpdateDetailsView();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
