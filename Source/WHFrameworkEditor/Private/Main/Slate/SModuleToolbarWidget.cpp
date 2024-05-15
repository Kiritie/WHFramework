// Fill out your copyright notice in the Description page of Project Settings.

#include "Main/Slate/SModuleToolbarWidget.h"

#include "SlateOptMacros.h"
#include "Main/Slate/SModuleEditorWidget.h"
#include "Main/Slate/SModuleListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

FName SModuleToolbarWidget::WidgetName = FName("ModuleToolbarWidget");

SModuleToolbarWidget::SModuleToolbarWidget()
{
	WidgetType = EEditorWidgetType::Child;
}

void SModuleToolbarWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
}

void SModuleToolbarWidget::OnPreviewToggled()
{
	GetParentWidgetN<SModuleEditorWidget>()->TogglePreview();
}

void SModuleToolbarWidget::OnDefaultsToggled()
{
	GetParentWidgetN<SModuleEditorWidget>()->ListWidget->ToggleDefaults();
}

void SModuleToolbarWidget::OnEditingToggled()
{
	GetParentWidgetN<SModuleEditorWidget>()->ListWidget->ToggleEditing();
}

void SModuleToolbarWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();

	FSlimHorizontalToolBarBuilder ToolBarBuilder_List = FSlimHorizontalToolBarBuilder(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None);

	ToolBarBuilder_List.SetStyle(&FAppStyle::Get(), "StatusBarToolBar");
	
	ToolBarBuilder_List.BeginSection("Editor");
	{
		ToolBarBuilder_List.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &SModuleToolbarWidget::OnPreviewToggled),
				FCanExecuteAction::CreateLambda([]() -> bool
				{
					return UCommonStatics::IsPlaying();
				}),
				FGetActionCheckState::CreateLambda([this](){
					return GetParentWidgetN<SModuleEditorWidget>()->bPreview ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			),
			NAME_None,
			FText::FromString(TEXT("Preview")),
			FText::FromString(TEXT("Toggle Preview")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Visibility"),
			EUserInterfaceActionType::ToggleButton
		);
	}
	ToolBarBuilder_List.EndSection();

	ToolBarBuilder_List.BeginSection("List");
	{
		ToolBarBuilder_List.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &SModuleToolbarWidget::OnDefaultsToggled),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this](){
					return GetParentWidgetN<SModuleEditorWidget>()->ListWidget->bDefaults ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			),
			NAME_None,
			FText::FromString(TEXT("Defaults")),
			FText::FromString(TEXT("Toggle Defaults")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "FullBlueprintEditor.EditGlobalOptions"),
			EUserInterfaceActionType::ToggleButton
		);
		ToolBarBuilder_List.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &SModuleToolbarWidget::OnEditingToggled),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this](){
					return GetParentWidgetN<SModuleEditorWidget>()->ListWidget->bEditing ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			),
			NAME_None,
			FText::FromString(TEXT("Editing")),
			FText::FromString(TEXT("Toggle Editing")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"),
			EUserInterfaceActionType::ToggleButton
		);
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
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					ToolBarBuilder_List.MakeWidget()
				]
			]
		]
	];
}

void SModuleToolbarWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SModuleToolbarWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();
}

void SModuleToolbarWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
