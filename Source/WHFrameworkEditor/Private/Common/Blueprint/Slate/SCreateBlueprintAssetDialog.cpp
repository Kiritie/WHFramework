// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/Blueprint/Slate/SCreateBlueprintAssetDialog.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "EditorDirectories.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "SPrimaryButton.h"
#include "Common/Blueprint/BlueprintFactoryBase.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "CreateBlueprintAssetDialog"

SCreateBlueprintAssetDialog::SCreateBlueprintAssetDialog()
{
	WindowTitle = LOCTEXT("CreateBlueprintAssetDialog_Title", "Create New Blueprint Asset");
	WindowSize = FVector2D(450, 450);

	AssetPath = TEXT("");
	AssetName = TEXT("");
	PackageName = TEXT("");

	BlueprintFactory = nullptr;
	UserResponse = EAppReturnType::No;
	
	BlueprintAsset = nullptr;

	bAutoOpenAsset = false;
}

void SCreateBlueprintAssetDialog::Construct(const FArguments& InArgs)
{
	BlueprintFactory = NewObject<UBlueprintFactoryBase>(GetTransientPackage(), InArgs._BlueprintFactoryClass);

	bAutoOpenAsset = InArgs._IsAutoOpenAsset;

	WindowTitle = FText::FromString(FString::Printf(TEXT("Create %s"), *BlueprintFactory->GetSupportedClass()->GetName()));

	const FString DefaultFilesystemDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::NEW_ASSET);
	if (DefaultFilesystemDirectory.IsEmpty() || !FPackageName::TryConvertFilenameToLongPackageName(DefaultFilesystemDirectory, AssetPath) || AssetPath.EndsWith(TEXT("/")))
	{
		AssetPath = TEXT("/Game");
	}

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = AssetPath;
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SCreateBlueprintAssetDialog::OnPathChange);
	PathPickerConfig.bAddDefaultPath = true;

	const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	SEditorWindowBase::Construct(SEditorWindowBase::FArguments()
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
						.Text(LOCTEXT("SelectPath", "Select Path:"))
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
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(2)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock).Text(LOCTEXT("AssetName", "Asset Name:"))
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
					.OnTextChanged(this, &SCreateBlueprintAssetDialog::SetAssetName)
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(8.f, 16.f)
			[
				SNew(SUniformGridPanel)
				.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))

				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SPrimaryButton)
					.Text(LOCTEXT("OK", "OK"))
					.OnClicked(this, &SCreateBlueprintAssetDialog::OnButtonClick, EAppReturnType::Ok)
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton)
					.Text(LOCTEXT("Cancel", "Cancel"))
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &SCreateBlueprintAssetDialog::OnButtonClick, EAppReturnType::Cancel)
				]
			]
		])
	);

	OnPathChange(AssetPath);
}

void SCreateBlueprintAssetDialog::OnPathChange(const FString& NewPath)
{
	IAssetTools* AssetTools = &FModuleManager::GetModuleChecked<FAssetToolsModule>(FName("AssetTools")).Get();

	AssetPath = NewPath;

	AssetTools->CreateUniqueAssetName(NewPath / BlueprintFactory->GetDefaultNewAssetName(), FString(), PackageName, AssetName);
}

FReply SCreateBlueprintAssetDialog::OnButtonClick(EAppReturnType::Type ButtonID)
{
	UserResponse = ButtonID;

	if(ButtonID != EAppReturnType::Cancel)
	{
		IAssetTools* AssetTools = &FModuleManager::GetModuleChecked<FAssetToolsModule>(FName("AssetTools")).Get();

		BlueprintAsset = Cast<UBlueprint>(AssetTools->CreateAsset(AssetName, AssetPath, BlueprintFactory->SupportedClass, BlueprintFactory, FName("ContentBrowserNewAsset")));
		if(BlueprintAsset)
		{
			// TArray<UObject*> ObjectsToSyncTo;
			// ObjectsToSyncTo.Add(BlueprintAsset);
			//
			// GEditor->SyncBrowserToObjects(ObjectsToSyncTo);

			FEditorDirectories::Get().SetLastDirectory(ELastDirectory::NEW_ASSET, AssetPath);

			if(bAutoOpenAsset)
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(BlueprintAsset, EToolkitMode::Standalone);
			}
		}
	}

	RequestDestroyWindow();

	return FReply::Handled();
}

void SCreateBlueprintAssetDialog::SetAssetName(const FText& InText)
{
	AssetName = InText.ToString();
}

EAppReturnType::Type SCreateBlueprintAssetDialog::ShowModal()
{
	if(BlueprintFactory->ConfigureProperties())
	{
		GEditor->EditorAddModalWindow(SharedThis(this));
	}
	else
	{
		OnButtonClick(EAppReturnType::Cancel);
	}
	return UserResponse;
}

#undef LOCTEXT_NAMESPACE
