// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/Asset/AssetFactoryBase.h"

#include "ClassViewerModule.h"
#include "Editor.h"
#include "..\..\..\Public\Common\CommonEditorTypes.h"

#include "Modules/ModuleManager.h"

#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/AppStyle.h"
#include "Input/Reply.h"
#include "Kismet2/SClassPickerDialog.h"

#define LOCTEXT_NAMESPACE "AssetFactoryBase"

UAssetFactoryBase::UAssetFactoryBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	
	SupportedClass = UObject::StaticClass();
	ParentClass = UObject::StaticClass();
	PickedClass = UObject::StaticClass();

	ClassPickerTitle = LOCTEXT("CreateAssetOptions", "Pick Class For Asset Instance");
}

bool UAssetFactoryBase::ConfigureProperties()
{
	// nullptr the AssetClass so we can check for selection
	PickedClass = nullptr;

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::TreeView;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	
	const TSharedPtr<FClassViewerFilterBase> Filter = MakeShareable(new FClassViewerFilterBase);
	Filter->DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown;
	Filter->AllowedChildrenOfClasses.Add(ParentClass);
	
	Options.ClassFilters.Add(Filter.ToSharedRef());

	Options.bExpandAllNodes = true;
	Options.bShowDefaultClasses = false;

	UClass* ChosenClass = nullptr;
	const bool bPressedOk = SClassPickerDialog::PickClass(ClassPickerTitle, Options, ChosenClass, SupportedClass);

	if ( bPressedOk )
	{
		PickedClass = ChosenClass;
	}

	return bPressedOk;
}

UObject* UAssetFactoryBase::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return Super::FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, CallingContext);
}

UObject* UAssetFactoryBase::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if (PickedClass != nullptr)
	{
		return NewObject<UObject>(InParent, PickedClass, Name, Flags | RF_Transactional);
	}
	else
	{
		// if we have no data asset class, use the passed-in class instead
		check(Class->IsChildOf(SupportedClass));
		return NewObject<UObject>(InParent, Class, Name, Flags);
	}
}

#undef LOCTEXT_NAMESPACE // "AssetFactoryBase"
