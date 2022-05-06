// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Widget/Window/SCreateStepBlueprintDialog.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "Debug/DebugModuleTypes.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "SelectPathDialog"

/////////////////////////////////////////////////
// select folder dialog
//////////////////////////////////////////////////
SCreateStepBlueprintDialog::SCreateStepBlueprintDialog()
{
	BlueprintFactory = nullptr;
	UserResponse = EAppReturnType::Cancel;

	WindowTitle = LOCTEXT("SCreateStepBlueprintDialog_Title", "Create New Step Blueprint");
	WindowSize = FVector2D(450, 450);
}

void SCreateStepBlueprintDialog::Construct(const FArguments& InArgs)
{
	AssetPath = InArgs._DefaultAssetPath/*FText::FromString(FPackageName::GetLongPackagePath(InArgs._DefaultAssetPath.ToString()))*/;

	BlueprintFactory = InArgs._BlueprintFactory;

	if(AssetPath.IsEmpty())
	{
		AssetPath = FText::FromString(TEXT("/Game"));
	}

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = AssetPath.ToString();
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SCreateStepBlueprintDialog::OnPathChange);
	PathPickerConfig.bAddDefaultPath = true;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	SEditorWindowWidgetBase::Construct(SEditorWindowWidgetBase::FArguments()
		.WindowArgs(SWindow::FArguments()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot() // Add user input block
			.Padding(2)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectPath", "Select Path"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					]

					+ SVerticalBox::Slot()
					.FillHeight(1)
					.Padding(3)
					[
						ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
					]
				]
			]

			+ SVerticalBox::Slot()
			.Padding(2)
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(2)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock).Text(LOCTEXT("SelectPathDialog_AssetName", "Asset Name: "))
					]

					+ SHorizontalBox::Slot()
					.Padding(2)
					.HAlign(HAlign_Fill)
					.FillWidth(1.f)
					[
						SNew(SEditableTextBox)
						.Text_Lambda([this](){ return FText::FromString(AssetName); })
						.IsReadOnly(false)
						.RevertTextOnEscape(true)
						.OnTextChanged(this, &SCreateStepBlueprintDialog::SetAssetName)
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(5)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("OK", "OK"))
					.OnClicked(this, &SCreateStepBlueprintDialog::OnButtonClick, EAppReturnType::Ok)
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked(this, &SCreateStepBlueprintDialog::OnButtonClick, EAppReturnType::Cancel)
				]
			]
		])
	);

	OnPathChange(AssetPath.ToString());
}

void SCreateStepBlueprintDialog::OnPathChange(const FString& NewPath)
{
	IAssetTools* AssetTools = &FModuleManager::GetModuleChecked<FAssetToolsModule>(FName("AssetTools")).Get();

	AssetPath = FText::FromString(NewPath);

	AssetTools->CreateUniqueAssetName(NewPath / BlueprintFactory->GetDefaultNewAssetName(), FString(), PackageName, AssetName);
}

FReply SCreateStepBlueprintDialog::OnButtonClick(EAppReturnType::Type ButtonID)
{
	UserResponse = ButtonID;

	RequestDestroyWindow();

	return FReply::Handled();
}

void SCreateStepBlueprintDialog::SetAssetName(const FText& InText)
{
	AssetName = InText.ToString();
}


EAppReturnType::Type SCreateStepBlueprintDialog::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

FString SCreateStepBlueprintDialog::GetAssetPath()
{
	return AssetPath.ToString();
}

FString SCreateStepBlueprintDialog::GetAssetName()
{
	return AssetName;
}

FString SCreateStepBlueprintDialog::GetPackageName()
{
	return PackageName;
}

#undef LOCTEXT_NAMESPACE
