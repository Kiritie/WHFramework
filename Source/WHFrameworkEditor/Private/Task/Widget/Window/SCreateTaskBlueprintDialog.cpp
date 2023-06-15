// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/Widget/Window/SCreateTaskBlueprintDialog.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "EditorStyleSet.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "Debug/DebugModuleTypes.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "SelectPathDialog"

/////////////////////////////////////////////////
// select folder dialog
//////////////////////////////////////////////////
SCreateTaskBlueprintDialog::SCreateTaskBlueprintDialog()
{
	BlueprintFactory = nullptr;
	UserResponse = EAppReturnType::Cancel;

	WindowTitle = LOCTEXT("SCreateTaskBlueprintDialog_Title", "Create New Task Blueprint");
	WindowSize = FVector2D(450, 450);
}

void SCreateTaskBlueprintDialog::Construct(const FArguments& InArgs)
{
	AssetPath = InArgs._DefaultAssetPath/*FText::FromString(FPackageName::GetLongPackagePath(InArgs._DefaultAssetPath.ToString()))*/;

	BlueprintFactory = InArgs._BlueprintFactory;

	if(AssetPath.IsEmpty())
	{
		AssetPath = FText::FromString(TEXT("/Game"));
	}

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = AssetPath.ToString();
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SCreateTaskBlueprintDialog::OnPathChange);
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
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
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
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
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
						.OnTextChanged(this, &SCreateTaskBlueprintDialog::SetAssetName)
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(5)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("OK", "OK"))
					.OnClicked(this, &SCreateTaskBlueprintDialog::OnButtonClick, EAppReturnType::Ok)
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked(this, &SCreateTaskBlueprintDialog::OnButtonClick, EAppReturnType::Cancel)
				]
			]
		])
	);

	OnPathChange(AssetPath.ToString());
}

void SCreateTaskBlueprintDialog::OnPathChange(const FString& NewPath)
{
	IAssetTools* AssetTools = &FModuleManager::GetModuleChecked<FAssetToolsModule>(FName("AssetTools")).Get();

	AssetPath = FText::FromString(NewPath);

	AssetTools->CreateUniqueAssetName(NewPath / BlueprintFactory->GetDefaultNewAssetName(), FString(), PackageName, AssetName);
}

FReply SCreateTaskBlueprintDialog::OnButtonClick(EAppReturnType::Type ButtonID)
{
	UserResponse = ButtonID;

	RequestDestroyWindow();

	return FReply::Handled();
}

void SCreateTaskBlueprintDialog::SetAssetName(const FText& InText)
{
	AssetName = InText.ToString();
}


EAppReturnType::Type SCreateTaskBlueprintDialog::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

FString SCreateTaskBlueprintDialog::GetAssetPath()
{
	return AssetPath.ToString();
}

FString SCreateTaskBlueprintDialog::GetAssetName()
{
	return AssetName;
}

FString SCreateTaskBlueprintDialog::GetPackageName()
{
	return PackageName;
}

#undef LOCTEXT_NAMESPACE