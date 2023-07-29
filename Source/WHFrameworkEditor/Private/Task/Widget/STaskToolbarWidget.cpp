// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/Widget/STaskToolbarWidget.h"

#include "SlateOptMacros.h"
#include "WHFrameworkEditorStyle.h"
#include "Task/TaskEditorSettings.h"
#include "Task/Widget/STaskToolbarWidget.h"

#include "Global/GlobalBPLibrary.h"
#include "Task/Widget/STaskEditorWidget.h"
#include "Task/Widget/STaskListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

STaskToolbarWidget::STaskToolbarWidget()
{
	WidgetName = FName("TaskToolbarWidget");
	WidgetType = EEditorWidgetType::Child;
}

void STaskToolbarWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	MainWidget = InArgs._MainWidget;
	ListWidget = InArgs._ListWidget;

	if(!MainWidget || !ListWidget) return;

	FSlateIcon Icon(FName("WidgetTaskEditorStyle"), "Icon.Empty");

	FToolBarBuilder ToolBarBuilder_Editor(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None);

	ToolBarBuilder_Editor.BeginSection("Editor");
	{
		#if WITH_SLATE_DEBUGGING
		ToolBarBuilder_Editor.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &STaskToolbarWidget::OnPreviewModeToggled),
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
				FExecuteAction::CreateRaw(this, &STaskToolbarWidget::OnMultiModeToggled),
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
				FExecuteAction::CreateRaw(this, &STaskToolbarWidget::OnEditModeToggled),
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

void STaskToolbarWidget::OnPreviewModeToggled()
{
	MainWidget->TogglePreviewMode();
}

void STaskToolbarWidget::OnMultiModeToggled()
{
	ListWidget->ToggleMultiMode();
}

void STaskToolbarWidget::OnEditModeToggled()
{
	ListWidget->ToggleEditMode();
}

void STaskToolbarWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void STaskToolbarWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void STaskToolbarWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();
}

void STaskToolbarWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
