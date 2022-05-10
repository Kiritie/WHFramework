// Fill out your copyright notice in the Description page of Project Settings.


#include "Global/Blueprint/Widget/SCreateBlueprintAssetDialog.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "Debug/DebugModuleTypes.h"
#include "Global/Blueprint/Factory/BlueprintFactoryBase.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "CreateBlueprintAssetDialog"

SCreateBlueprintAssetDialog::SCreateBlueprintAssetDialog()
{
	BlueprintFactory = nullptr;
	UserResponse = EAppReturnType::Cancel;

	WindowTitle = LOCTEXT("CreateBlueprintAssetDialog_Title", "Create New Blueprint Asset");
	WindowSize = FVector2D(450, 450);
}

void SCreateBlueprintAssetDialog::Construct(const FArguments& InArgs)
{
	AssetPath = InArgs._DefaultAssetPath/*FText::FromString(FPackageName::GetLongPackagePath(InArgs._DefaultAssetPath.ToString()))*/;

	BlueprintFactory = InArgs._BlueprintFactory;

	WindowTitle = FText::FromString(FString::Printf(TEXT("Create %s"), *BlueprintFactory->GetSupportedClass()->GetName()));

	if(AssetPath.IsEmpty())
	{
		AssetPath = FText::FromString(TEXT("/Game"));
	}

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = AssetPath.ToString();
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SCreateBlueprintAssetDialog::OnPathChange);
	PathPickerConfig.bAddDefaultPath = true;

	SelectedClass = BlueprintFactory->ParentClass;

	ClassViewerOptions.bShowBackgroundBorder = false;
	ClassViewerOptions.bShowUnloadedBlueprints = true;
	ClassViewerOptions.bShowNoneOption = false;

	ClassViewerOptions.bIsBlueprintBaseOnly = false;
	ClassViewerOptions.bIsPlaceableOnly = false;
	ClassViewerOptions.NameTypeToDisplay = EClassViewerNameTypeToDisplay::ClassName;
	ClassViewerOptions.DisplayMode = EClassViewerDisplayMode::ListView;
	ClassViewerOptions.bAllowViewOptions = true;

	ClassFilter = MakeShareable(new FBlueprintParentClassFilter);
	ClassFilter->IncludeParentClass = BlueprintFactory->ParentClass;
	#if ENGINE_MAJOR_VERSION == 4
	ClassViewerOptions.ClassFilter = ClassFilter;
	#else if ENGINE_MAJOR_VERSION == 5
	ClassViewerOptions.ClassFilters.Add(ClassFilter.ToSharedRef());
	#endif

	SAssignNew(ClassPickButton, SComboButton)
		.OnGetMenuContent(this, &SCreateBlueprintAssetDialog::GenerateClassPicker)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.ToolTipText(this, &SCreateBlueprintAssetDialog::GetPickedClassName)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &SCreateBlueprintAssetDialog::GetPickedClassName)
		];

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
					SNew(STextBlock).Text(LOCTEXT("ParentClass", "Parent Class:"))
				]

				+ SHorizontalBox::Slot()
				.Padding(2)
				.HAlign(HAlign_Fill)
				.FillWidth(1.f)
				[
					ClassPickButton.ToSharedRef()
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
					.IsEnabled_Lambda([this](){ return SelectedClass != nullptr; })
					.OnClicked(this, &SCreateBlueprintAssetDialog::OnButtonClick, EAppReturnType::Ok)
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked(this, &SCreateBlueprintAssetDialog::OnButtonClick, EAppReturnType::Cancel)
				]
			]
		])
	);

	OnPathChange(AssetPath.ToString());
}

void SCreateBlueprintAssetDialog::OnPathChange(const FString& NewPath)
{
	IAssetTools* AssetTools = &FModuleManager::GetModuleChecked<FAssetToolsModule>(FName("AssetTools")).Get();

	AssetPath = FText::FromString(NewPath);

	AssetTools->CreateUniqueAssetName(NewPath / BlueprintFactory->GetDefaultNewAssetName(), FString(), PackageName, AssetName);
}

FReply SCreateBlueprintAssetDialog::OnButtonClick(EAppReturnType::Type ButtonID)
{
	UserResponse = ButtonID;

	RequestDestroyWindow();

	return FReply::Handled();
}

void SCreateBlueprintAssetDialog::SetAssetName(const FText& InText)
{
	AssetName = InText.ToString();
}


EAppReturnType::Type SCreateBlueprintAssetDialog::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

TSharedRef<SWidget> SCreateBlueprintAssetDialog::GenerateClassPicker()
{
	FOnClassPicked OnPicked(FOnClassPicked::CreateRaw(this, &SCreateBlueprintAssetDialog::OnClassPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(ClassViewerOptions, OnPicked)
			]
		];
}

void SCreateBlueprintAssetDialog::OnClassPicked(UClass* InClass)
{
	ClassPickButton->SetIsOpen(false);

	SelectedClass = InClass;

	BlueprintFactory->ParentClass = SelectedClass;
}

FText SCreateBlueprintAssetDialog::GetPickedClassName() const
{
	return FText::FromString(SelectedClass ? SelectedClass->GetName() : TEXT("None"));
}

FString SCreateBlueprintAssetDialog::GetAssetPath()
{
	return AssetPath.ToString();
}

FString SCreateBlueprintAssetDialog::GetAssetName()
{
	return AssetName;
}

FString SCreateBlueprintAssetDialog::GetPackageName()
{
	return PackageName;
}

#undef LOCTEXT_NAMESPACE
