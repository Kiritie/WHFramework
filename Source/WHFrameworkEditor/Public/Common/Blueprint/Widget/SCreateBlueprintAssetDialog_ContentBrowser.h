// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "Common/Blueprint/Factory/BlueprintFactoryBase.h"

class FBlueprintParentClassFilter_ContentBrowser : public IClassViewerFilter
{
public:
	/** All children of these classes will be included unless filtered out by another setting. */
	TSet<const UClass*> AllowedChildrenOfClasses;

	FBlueprintParentClassFilter_ContentBrowser() { }

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

class SCreateBlueprintAssetDialog_ContentBrowser : public SCompoundWidget
{
public:
	SCreateBlueprintAssetDialog_ContentBrowser();
	
	SLATE_BEGIN_ARGS(SCreateBlueprintAssetDialog_ContentBrowser) { }

		SLATE_ARGUMENT(TWeakObjectPtr<UClass>, SupportedClass)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	/** Sets properties for the supplied BlueprintFactory */
	bool ConfigureProperties(TWeakObjectPtr<UBlueprintFactoryBase> InBlueprintFactory);

private:
	/** Creates the combo menu for the parent class */
	void MakeParentClassPicker();

	/** Handler for when a parent class is selected */
	void OnClassPicked(UClass* ChosenClass) { ParentClass = ChosenClass; }

	/** Handler for when ok is clicked */
	FReply OkClicked();

	void CloseDialog(bool bWasPicked = false);

	/** Handler for when cancel is clicked */
	FReply CancelClicked();

	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

private:
	/** The factory for which we are setting up properties */
	TWeakObjectPtr<UBlueprintFactoryBase> BlueprintFactory;

	/** A pointer to the window that is asking the user to select a parent class */
	TWeakPtr<SWindow> PickerWindow;

	/** The container for the Parent Class picker */
	TSharedPtr<SVerticalBox> ParentClassContainer;

	/** The selected class */
	TWeakObjectPtr<UClass> SupportedClass;

	/** The selected class */
	TWeakObjectPtr<UClass> ParentClass;

	/** True if Ok was clicked */
	bool bOkClicked;
};
