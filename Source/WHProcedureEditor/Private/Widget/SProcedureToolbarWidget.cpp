// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/SProcedureToolbarWidget.h"

#include "SlateOptMacros.h"
#include "Widget/SProcedureListWidget.h"
#include "Widget/SProcedureMainWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProcedureToolbarWidget::Construct(const FArguments& InArgs)
{
	MainWidget = InArgs._MainWidget;
	ListWidget = InArgs._ListWidget;
	
	if(!MainWidget || !ListWidget) return;

	FSlateIcon Icon(FName("WidgetProcedureEditorStyle"), "Icon.Empty");

	FToolBarBuilder ToolBarBuilder_View(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None);

	ToolBarBuilder_View.BeginSection("View");
	{
#if WITH_SLATE_DEBUGGING
		ToolBarBuilder_View.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					MainWidget->bShowListPanel = !MainWidget->bShowListPanel;
					GConfig->SetBool(TEXT("/Script/WHProcedureEditor.WHProcedureEditorSettings"), TEXT("bShowListPanel"), MainWidget->bShowListPanel, GProcedureEditorIni);
				}),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this]()
				{
					return MainWidget->bShowListPanel ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			),
			NAME_None,
			FText::FromString(TEXT("Procedure List")),
			FText::FromString(TEXT("Show/Hide Procedure List")),
			Icon,
			EUserInterfaceActionType::ToggleButton
		);
		ToolBarBuilder_View.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					MainWidget->bShowDetailPanel = !MainWidget->bShowDetailPanel;
					GConfig->SetBool(TEXT("/Script/WHProcedureEditor.WHProcedureEditorSettings"), TEXT("bShowDetailPanel"), MainWidget->bShowDetailPanel, GProcedureEditorIni);
				}),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this]()
				{
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
				FExecuteAction::CreateLambda([this]()
				{
					MainWidget->bShowStatusPanel = !MainWidget->bShowStatusPanel;
					GConfig->SetBool(TEXT("/Script/WHProcedureEditor.WHProcedureEditorSettings"), TEXT("bShowStatusPanel"), MainWidget->bShowStatusPanel, GProcedureEditorIni);
				}),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this]()
				{
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
				FExecuteAction::CreateRaw(this, &SProcedureToolbarWidget::OnMultiModeToggled),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this]()
				{
					return ListWidget->bCurrentIsMultiMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
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
				FGetActionCheckState::CreateLambda([this]()
				{
					return ListWidget->bCurrentIsEditMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
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

void SProcedureToolbarWidget::OnEditModeToggled()
{
	ListWidget->ToggleEditMode();
}

void SProcedureToolbarWidget::OnMultiModeToggled()
{
	ListWidget->ToggleMultiMode();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION