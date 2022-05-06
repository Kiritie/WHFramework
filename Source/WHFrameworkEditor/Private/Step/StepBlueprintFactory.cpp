// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/StepBlueprintFactory.h"

#include "BlueprintEditorSettings.h"
#include "ClassViewerFilter.h"
#include "K2Node_Event.h"
#include "SlateOptMacros.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Step/StepGraph.h"
#include "Step/StepGraphSchema.h"
#include "Step/Base/StepBase.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Step/Base/StepBlueprint.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Debug/DebugModuleTypes.h"

//#include "EdGraphSchema_K2.h"

#define LOCTEXT_NAMESPACE "UStepBlueprintFactory"

// ------------------------------------------------------------------------------
// Dialog to configure creation properties
// ------------------------------------------------------------------------------
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

class SStepBlueprintCreateDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStepBlueprintCreateDialog) { }

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs)
	{
		bOkClicked = false;
		ParentClass = UStepBase::StaticClass();

		ChildSlot
		[
			SNew(SBorder)
			.Visibility(EVisibility::Visible)
			.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
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
						.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
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
						.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
						.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
						.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
						+ SUniformGridPanel::Slot(0, 0)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
							.OnClicked(this, &SStepBlueprintCreateDialog::OkClicked)
							.Text(LOCTEXT("CreateStepBlueprintOk", "OK"))
						]
						+ SUniformGridPanel::Slot(1, 0)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
							.OnClicked(this, &SStepBlueprintCreateDialog::CancelClicked)
							.Text(LOCTEXT("CreateStepBlueprintCancel", "Cancel"))
						]
					]
				]
			]
		];

		MakeParentClassPicker();
	}

	/** Sets properties for the supplied StepBlueprintFactory */
	bool ConfigureProperties(TWeakObjectPtr<UStepBlueprintFactory> InStepBlueprintFactory)
	{
		StepBlueprintFactory = InStepBlueprintFactory;

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("CreateStepBlueprintOptions", "Create Step Blueprint"))
			.ClientSize(FVector2D(400, 700))
			.SupportsMinimize(false).SupportsMaximize(false)
			[
				AsShared()
			];

		PickerWindow = Window;

		GEditor->EditorAddModalWindow(Window);
		StepBlueprintFactory.Reset();

		return bOkClicked;
	}

private:
	class FStepBlueprintParentFilter : public IClassViewerFilter
	{
	public:
		/** All children of these classes will be included unless filtered out by another setting. */
		TSet<const UClass*> AllowedChildrenOfClasses;

		FStepBlueprintParentFilter() { }

		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
		{
			// If it appears on the allowed child-of classes list (or there is nothing on that list)
			return InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
		{
			// If it appears on the allowed child-of classes list (or there is nothing on that list)
			return InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;
		}
	};

	/** Creates the combo menu for the parent class */
	void MakeParentClassPicker()
	{
		// Load the classviewer module to display a class picker
		FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

		// Fill in options
		FClassViewerInitializationOptions Options;
		Options.Mode = EClassViewerMode::ClassPicker;

		// Only allow parenting to base blueprints.
		Options.bIsBlueprintBaseOnly = true;

		TSharedPtr<FStepBlueprintParentFilter> Filter = MakeShareable(new FStepBlueprintParentFilter);

		// All child child classes of UStepBase are valid.
		Filter->AllowedChildrenOfClasses.Add(UStepBase::StaticClass());
		Options.ClassFilter = Filter;

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
			ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SStepBlueprintCreateDialog::OnClassPicked))
		];
	}

	/** Handler for when a parent class is selected */
	void OnClassPicked(UClass* ChosenClass) { ParentClass = ChosenClass; }

	/** Handler for when ok is clicked */
	FReply OkClicked()
	{
		if(StepBlueprintFactory.IsValid())
		{
			StepBlueprintFactory->BlueprintType = BPTYPE_Normal;
			StepBlueprintFactory->ParentClass = ParentClass.Get();
		}

		CloseDialog(true);

		return FReply::Handled();
	}

	void CloseDialog(bool bWasPicked = false)
	{
		bOkClicked = bWasPicked;
		if(PickerWindow.IsValid()) { PickerWindow.Pin()->RequestDestroyWindow(); }
	}

	/** Handler for when cancel is clicked */
	FReply CancelClicked()
	{
		CloseDialog();
		return FReply::Handled();
	}

	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		if(InKeyEvent.GetKey() == EKeys::Escape)
		{
			CloseDialog();
			return FReply::Handled();
		}
		return SWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

private:
	/** The factory for which we are setting up properties */
	TWeakObjectPtr<UStepBlueprintFactory> StepBlueprintFactory;

	/** A pointer to the window that is asking the user to select a parent class */
	TWeakPtr<SWindow> PickerWindow;

	/** The container for the Parent Class picker */
	TSharedPtr<SVerticalBox> ParentClassContainer;

	/** The selected class */
	TWeakObjectPtr<UClass> ParentClass;

	/** True if Ok was clicked */
	bool bOkClicked;
};

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

/*------------------------------------------------------------------------------
	UStepBlueprintFactory implementation.
------------------------------------------------------------------------------*/

UStepBlueprintFactory::UStepBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UStepBlueprint::StaticClass();
	ParentClass = UStepBase::StaticClass();
}

bool UStepBlueprintFactory::ConfigureProperties()
{
	TSharedRef<SStepBlueprintCreateDialog> Dialog = SNew(SStepBlueprintCreateDialog);
	return Dialog->ConfigureProperties(this);
};

UObject* UStepBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a Step blueprint, then create and init one
	check(Class->IsChildOf(UStepBlueprint::StaticClass()));

	// If they selected an interface, force the parent class to be UInterface
	if(BlueprintType == BPTYPE_Interface) { ParentClass = UInterface::StaticClass(); }

	if((ParentClass == NULL) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass) || !ParentClass->IsChildOf(UStepBase::StaticClass()))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString(ParentClass->GetName()) : LOCTEXT("Null", "(null)"));
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("CannotCreateStepBlueprint", "Cannot create a Step Blueprint based on the class '{ClassName}'."), Args));
		return NULL;
	}
	else
	{
		UStepBlueprint* NewBP = CastChecked<UStepBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BlueprintType, UStepBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext));

		if(NewBP)
		{
			UStepBlueprint* StepBP = UStepBlueprint::FindRootStepBlueprint(NewBP);
			if(StepBP == NULL)
			{
				const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

				// Only allow a Step graph if there isn't one in a parent blueprint
				UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(NewBP, TEXT("Step Graph"), UStepGraph::StaticClass(), UStepGraphSchema::StaticClass());
				#if WITH_EDITORONLY_DATA
				if(NewBP->UbergraphPages.Num()) { FBlueprintEditorUtils::RemoveGraphs(NewBP, NewBP->UbergraphPages); }
				#endif
				FBlueprintEditorUtils::AddUbergraphPage(NewBP, NewGraph);
				NewBP->LastEditedDocuments.Add(NewGraph);
				NewGraph->bAllowDeletion = false;

				UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
				if(Settings && Settings->bSpawnDefaultBlueprintNodes)
				{
					int32 NodePositionY = 0;
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnInitialize")), UStepBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnRestore")), UStepBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnEnter")), UStepBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnRefresh")), UStepBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnGuide")), UStepBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnExecute")), UStepBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnComplete")), UStepBase::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("K2_OnLeave")), UStepBase::StaticClass(), NodePositionY);
				}
			}
		}

		return NewBP;
	}
}

UObject* UStepBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) { return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None); }

#undef LOCTEXT_NAMESPACE
