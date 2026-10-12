// Fill out your copyright notice in the Description page of Project Settings.

#include "Step/Slate/SStepDetailsWidget.h"

#include "SlateOptMacros.h"
#include "Step/Slate/SStepGraphWidget.h"
#include "Step/Base/StepBase.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

const FName SStepDetailsWidget::WidgetName = FName("StepDetailsWidget");

SStepDetailsWidget::SStepDetailsWidget()
{
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
	const TSharedPtr<FStepEditor> Editor = StepEditor.Pin();
	if (!Editor) return;
	TArray<UObject*> Objects;
	if (Editor->GraphWidget)
		for (UStepBase* Step : Editor->GraphWidget->GetSelectedSteps()) Objects.Add(Step);
	if (Objects.IsEmpty()) Objects.Add(Editor->GetEditingAsset());
	DetailsView->SetObjects(Objects);
}

void SStepDetailsWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
