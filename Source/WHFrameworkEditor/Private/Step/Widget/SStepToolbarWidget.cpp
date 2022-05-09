// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Widget/SStepToolbarWidget.h"

#include "SlateOptMacros.h"
#include "WHFrameworkEditorStyle.h"
#include "Step/StepEditorSettings.h"
#include "Step/Widget/SStepToolbarWidget.h"

#include "Step/Widget/SStepEditorWidget.h"
#include "Step/Widget/SStepListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SStepToolbarWidget::SStepToolbarWidget()
{
	WidgetName = FName("StepToolbarWidget");
	WidgetType = EEditorWidgetType::Child;
}

void SStepToolbarWidget::Construct(const FArguments& InArgs)
{
	SEditorSlateWidgetBase::Construct(SEditorSlateWidgetBase::FArguments());

	MainWidget = InArgs._MainWidget;
	ListWidget = InArgs._ListWidget;

	if(!MainWidget || !ListWidget) return;

	FSlateIcon Icon(FName("WidgetStepEditorStyle"), "Icon.Empty");

	FToolBarBuilder ToolBarBuilder_Editor(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None);

	ToolBarBuilder_Editor.BeginSection("Editor");
	{
		#if WITH_SLATE_DEBUGGING
		ToolBarBuilder_Editor.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &SStepToolbarWidget::OnPreviewModeToggled),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this](){
					return MainWidget->bPreviewMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			),
			NAME_None,
			FText::FromString(TEXT("Preview Mode")),
			FText::FromString(TEXT("Toggle Preview Mode")),
			Icon,
			EUserInterfaceActionType::ToggleButton
		);
		#endif
	}
	ToolBarBuilder_Editor.EndSection();

	FToolBarBuilder ToolBarBuilder_View(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None);

	ToolBarBuilder_View.BeginSection("View");
	{
		#if WITH_SLATE_DEBUGGING
		ToolBarBuilder_View.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateLambda([this](){
					MainWidget->bShowListPanel = !MainWidget->bShowListPanel;
					GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bShowListPanel"), MainWidget->bShowListPanel, GStepEditorIni);
				}),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this](){
					return MainWidget->bShowListPanel ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			),
			NAME_None,
			FText::FromString(TEXT("Step List")),
			FText::FromString(TEXT("Show/Hide Step List")),
			Icon,
			EUserInterfaceActionType::ToggleButton
		);
		ToolBarBuilder_View.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateLambda([this](){
					MainWidget->bShowDetailPanel = !MainWidget->bShowDetailPanel;
					GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bShowDetailPanel"), MainWidget->bShowDetailPanel, GStepEditorIni);
				}),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this](){
					return MainWidget->bShowDetailPanel ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			),
			NAME_None,
			FText::FromString(TEXT("Detail Panel")),
			FText::FromString(TEXT("Show/Hide Detail Panel")),
			Icon,
			EUserInterfaceActionType::ToggleButton
		);
		ToolBarBuilder_View.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateLambda([this](){
					MainWidget->bShowStatusPanel = !MainWidget->bShowStatusPanel;
					GConfig->SetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bShowStatusPanel"), MainWidget->bShowStatusPanel, GStepEditorIni);
				}),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this](){
					return MainWidget->bShowStatusPanel ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			),
			NAME_None,
			FText::FromString(TEXT("Status Panel")),
			FText::FromString(TEXT("Show/Hide Status Panel")),
			Icon,
			EUserInterfaceActionType::ToggleButton
		);
		#endif
	}
	ToolBarBuilder_View.EndSection();

	FToolBarBuilder ToolBarBuilder_List(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None);

	ToolBarBuilder_List.BeginSection("List");
	{
		#if WITH_SLATE_DEBUGGING
		ToolBarBuilder_List.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &SStepToolbarWidget::OnMultiModeToggled),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this](){
					return ListWidget->bMultiMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			),
			NAME_None,
			FText::FromString(TEXT("Multi Mode")),
			FText::FromString(TEXT("Toggle Multi Mode")),
			Icon,
			EUserInterfaceActionType::ToggleButton
		);
		ToolBarBuilder_List.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &SStepToolbarWidget::OnEditModeToggled),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this](){
					return ListWidget->bEditMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			),
			NAME_None,
			FText::FromString(TEXT("Edit Mode")),
			FText::FromString(TEXT("Toggle Edit Mode")),
			Icon,
			EUserInterfaceActionType::ToggleButton
		);
		#endif
	}
	ToolBarBuilder_List.EndSection();

	ChildSlot
	[
		SNew(SBorder)
		.Padding(1.f)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				.Padding(5.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Editor:")))
					.ColorAndOpacity(FSlateColor(FLinearColor::Yellow))
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				.Padding(FMargin(5.f, 0.f))
				[
					ToolBarBuilder_Editor.MakeWidget()
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				.Padding(5.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("View:")))
					.ColorAndOpacity(FSlateColor(FLinearColor::Yellow))
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				.Padding(FMargin(5.f, 0.f))
				[
					ToolBarBuilder_View.MakeWidget()
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				.Padding(5.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("List:")))
					.ColorAndOpacity(FSlateColor(FLinearColor::Yellow))
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				.Padding(FMargin(5.f, 0.f))
				[
					ToolBarBuilder_List.MakeWidget()
				]
			]
		]
	];
}

void SStepToolbarWidget::OnPreviewModeToggled()
{
	MainWidget->TogglePreviewMode();
}

void SStepToolbarWidget::OnMultiModeToggled()
{
	ListWidget->ToggleMultiMode();
}

void SStepToolbarWidget::OnEditModeToggled()
{
	ListWidget->ToggleEditMode();
}

void SStepToolbarWidget::OnCreate()
{
	SEditorSlateWidgetBase::OnCreate();
}

void SStepToolbarWidget::OnReset()
{
	SEditorSlateWidgetBase::OnReset();
}

void SStepToolbarWidget::OnRefresh()
{
	SEditorSlateWidgetBase::OnRefresh();
}

void SStepToolbarWidget::OnDestroy()
{
	SEditorSlateWidgetBase::OnDestroy();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
