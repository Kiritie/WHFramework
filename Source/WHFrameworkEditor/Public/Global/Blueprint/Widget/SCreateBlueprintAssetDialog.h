// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "Step/StepEditorTypes.h"
#include "Widget/Window/SEditorWindowBase.h"

class UBlueprintFactoryBase;

class FBlueprintParentClassFilter : public IClassViewerFilter
{
public:
	const UClass* IncludeParentClass;
	const UClass* UnIncludeParentClass;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		return IsClassAllowedHelper(InClass);
	}
	
	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InBlueprint, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return IsClassAllowedHelper(InBlueprint);
	}

private:
	template <typename TClass>
	bool IsClassAllowedHelper(TClass InClass)
	{
		return InClass->IsChildOf(IncludeParentClass) && !InClass->IsChildOf(UnIncludeParentClass);
	}
};

class SCreateBlueprintAssetDialog : public SEditorWindowBase
{
public:
	SCreateBlueprintAssetDialog();

	SLATE_BEGIN_ARGS(SCreateBlueprintAssetDialog) { }

		SLATE_ARGUMENT(FText, DefaultAssetPath)
		SLATE_ARGUMENT(UBlueprintFactoryBase*, BlueprintFactory)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	/** Displays the dialog in a blocking fashion */
	EAppReturnType::Type ShowModal();

	/** Gets the resulting asset path */
	FString GetAssetPath();

	/** Gets the resulting asset name */
	FString GetAssetName();

	/** Gets the resulting package name */
	FString GetPackageName();

protected:
	void OnPathChange(const FString& NewPath);
	
	FReply OnButtonClick(EAppReturnType::Type ButtonID);
	
	void SetAssetName(const FText &InText);

	TSharedRef<SWidget> GenerateClassPicker();

	void OnClassPicked(UClass* InClass);

	FText GetPickedClassName() const;

	FText AssetPath;
	FString AssetName;
	FString PackageName;

	UBlueprintFactoryBase* BlueprintFactory;

	EAppReturnType::Type UserResponse;

	TSubclassOf<UObject> SelectedClass;

	TSharedPtr<SComboButton> ClassPickButton;

	FClassViewerInitializationOptions ClassViewerOptions;

	TSharedPtr<FBlueprintParentClassFilter> ClassFilter;
};
