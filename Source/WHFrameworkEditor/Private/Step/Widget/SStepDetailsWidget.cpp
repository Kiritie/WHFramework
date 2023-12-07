// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Widget/SStepDetailsWidget.h"

#include "SlateOptMacros.h"
#include "Step/Base/StepBase.h"
#include "Step/Widget/SStepListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SStepDetailsWidget::SStepDetailsWidget()
{
	WidgetName = FName("StepDetailsWidget");
	WidgetType = EEditorWidgetType::Child;
}

void SStepDetailsWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
	
	StepEditor = InArgs._StepEditor;

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

void SStepDetailsWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void SStepDetailsWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SStepDetailsWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();

	if(StepEditor.Pin()->ListWidget->bDefaults)
	{
		DetailsView->SetObject(StepEditor.Pin()->GetEditingAsset());
	}
	else if(StepEditor.Pin()->ListWidget->SelectedStepListItems.Num() > 0)
	{
		TArray<UObject*> Steps = TArray<UObject*>();
		for(auto Iter : StepEditor.Pin()->ListWidget->SelectedStepListItems)
		{
			if(Iter->Step)
			{
				Steps.Add(Iter->Step);
			}
		}
		DetailsView->SetObjects(Steps);
	}
	else
	{
		DetailsView->SetObject(nullptr);
	}
}

void SStepDetailsWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
