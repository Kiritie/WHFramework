// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Widget/SProcedureToolbarWidget.h"

#include "SlateOptMacros.h"
#include "WHFrameworkEditorStyle.h"
#include "Procedure/ProcedureEditorSettings.h"
#include "Procedure/Widget/SProcedureToolbarWidget.h"

#include "Procedure/Widget/SProcedureEditorWidget.h"
#include "Procedure/Widget/SProcedureListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SProcedureToolbarWidget::SProcedureToolbarWidget()
{
	WidgetName = FName("ProcedureToolbarWidget");
	WidgetType = EEditorWidgetType::Child;
}

void SProcedureToolbarWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	MainWidget = InArgs._MainWidget;
	ListWidget = InArgs._ListWidget;

	if(!MainWidget || !ListWidget) return;

	FSlateIcon Icon(FName("WidgetProcedureEditorStyle"), "Icon.Empty");

	FToolBarBuilder ToolBarBuilder_Editor(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None);

	ToolBarBuilder_Editor.BeginSection("Editor");
	{
		#if WITH_SLATE_DEBUGGING
		ToolBarBuilder_Editor.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &SProcedureToolbarWidget::OnPreviewModeToggled),
				FCanExecuteAction::CreateLambda([]() -> bool
				{
					return UGlobalBPLibrary::IsPlaying();
				}),
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

	FToolBarBuilder ToolBarBuilder_List(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None);

	ToolBarBuilder_List.BeginSection("List");
	{
		#if WITH_SLATE_DEBUGGING
		ToolBarBuilder_List.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &SProcedureToolbarWidget::OnMultiModeToggled),
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
				FExecuteAction::CreateRaw(this, &SProcedureToolbarWidget::OnEditModeToggled),
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

void SProcedureToolbarWidget::OnPreviewModeToggled()
{
	MainWidget->TogglePreviewMode();
}

void SProcedureToolbarWidget::OnMultiModeToggled()
{
	ListWidget->ToggleMultiMode();
}

void SProcedureToolbarWidget::OnEditModeToggled()
{
	ListWidget->ToggleEditMode();
}

void SProcedureToolbarWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void SProcedureToolbarWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SProcedureToolbarWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();
}

void SProcedureToolbarWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
