// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/Base/Window/SEditorWindowBase.h"

class UFactory;

class SCreateStepBlueprintWindow : public SEditorWindowBase
{
public:
	SCreateStepBlueprintWindow();

	SLATE_BEGIN_ARGS(SCreateStepBlueprintWindow) { }

		SLATE_ARGUMENT(FText, DefaultAssetPath)
		SLATE_ARGUMENT(UFactory*, BlueprintFactory)

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

	UFactory* BlueprintFactory;
	
	FText AssetPath;
	FString AssetName;
	FString PackageName;

	EAppReturnType::Type UserResponse;
};
