// Fill out your copyright notice in the Description page of Project Settings.


#include "Global/Blueprint/Widget/SCreateBlueprintAssetDialog_ContentBrowser.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "EditorStyleSet.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "SlateOptMacros.h"
#include "Debug/DebugModuleTypes.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "CreateBlueprintAssetDialog"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SCreateBlueprintAssetDialog_ContentBrowser::SCreateBlueprintAssetDialog_ContentBrowser()
{
	BlueprintFactory = nullptr;

	bOkClicked = false;
}

void SCreateBlueprintAssetDialog_ContentBrowser::Construct(const FArguments& InArgs)
{
	SupportedClass = InArgs._SupportedClass;

	ChildSlot
	[
		SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(SBox)
			.Visibility(EVisibility::Visible)
			.WidthOverride(500.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Content()
					[
						SAssignNew(ParentClassContainer, SVerticalBox)
					]
				]

				// Ok/Cancel buttons
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(8)
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
						.OnClicked(this, &SCreateBlueprintAssetDialog_ContentBrowser::OkClicked)
						.Text(LOCTEXT("CreateBlueprintOk", "OK"))
					]
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
						.OnClicked(this, &SCreateBlueprintAssetDialog_ContentBrowser::CancelClicked)
						.Text(LOCTEXT("CreateBlueprintCancel", "Cancel"))
					]
				]
			]
		]
	];

	MakeParentClassPicker();
}

/** Sets properties for the supplied BlueprintFactory */
bool SCreateBlueprintAssetDialog_ContentBrowser::ConfigureProperties(TWeakObjectPtr<UBlueprintFactoryBase> InBlueprintFactory)
{
	BlueprintFactory = InBlueprintFactory;

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(FString::Printf(TEXT("Create %s"), *BlueprintFactory->GetSupportedClass()->GetName())))
		.ClientSize(FVector2D(400, 700))
		.SupportsMinimize(false).SupportsMaximize(false)
		[
			AsShared()
		];

	PickerWindow = Window;

	GEditor->EditorAddModalWindow(Window);
	BlueprintFactory.Reset();

	return bOkClicked;
}

/** Creates the combo menu for the parent class */
void SCreateBlueprintAssetDialog_ContentBrowser::MakeParentClassPicker()
{
	// Load the classviewer module to display a class picker
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;

	// Only allow parenting to base blueprints.
	Options.bIsBlueprintBaseOnly = true;

	TSharedPtr<FBlueprintParentClassFilter_ContentBrowser> Filter = MakeShareable(new FBlueprintParentClassFilter_ContentBrowser);

	// All child child classes of UBase are valid.
	Filter->AllowedChildrenOfClasses.Add(SupportedClass.Get());
	#if ENGINE_MAJOR_VERSION == 4
	Options.ClassFilter = Filter;
	#else if ENGINE_MAJOR_VERSION == 5
	Options.ClassFilters.Add(Filter.ToSharedRef());
	#endif

	ParentClassContainer->ClearChildren();
	ParentClassContainer->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ParentClass", "Parent Class:"))
			.ShadowOffset(FVector2D(1.0f, 1.0f))
		];

	ParentClassContainer->AddSlot()
	[
		ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SCreateBlueprintAssetDialog_ContentBrowser::OnClassPicked))
	];
}

/** Handler for when ok is clicked */
FReply SCreateBlueprintAssetDialog_ContentBrowser::OkClicked()
{
	if(BlueprintFactory.IsValid())
	{
		BlueprintFactory->BlueprintType = BPTYPE_Normal;
		BlueprintFactory->ParentClass = ParentClass.Get();
	}

	CloseDialog(true);

	return FReply::Handled();
}

void SCreateBlueprintAssetDialog_ContentBrowser::CloseDialog(bool bWasPicked)
{
	bOkClicked = bWasPicked;
	if(PickerWindow.IsValid()) { PickerWindow.Pin()->RequestDestroyWindow(); }
}

/** Handler for when cancel is clicked */
FReply SCreateBlueprintAssetDialog_ContentBrowser::CancelClicked()
{
	CloseDialog();
	return FReply::Handled();
}

FReply SCreateBlueprintAssetDialog_ContentBrowser::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if(InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseDialog();
		return FReply::Handled();
	}
	return SWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
