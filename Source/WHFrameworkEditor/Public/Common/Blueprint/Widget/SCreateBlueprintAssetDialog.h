// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Base/Window/SEditorWindowBase.h"

class UBlueprintFactoryBase;

class SCreateBlueprintAssetDialog : public SEditorWindowBase
{
public:
	SCreateBlueprintAssetDialog();

	SLATE_BEGIN_ARGS(SCreateBlueprintAssetDialog) { }

		SLATE_ARGUMENT(TSubclassOf<UBlueprintFactoryBase>, BlueprintFactoryClass)
		SLATE_ARGUMENT(bool, IsAutoOpenAsset)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	/** Displays the dialog in a blocking fashion */
	EAppReturnType::Type ShowModal();

protected:
	void OnPathChange(const FString& NewPath);
	
	FReply OnButtonClick(EAppReturnType::Type ButtonID);
	
	void SetAssetName(const FText &InText);

protected:
	FString AssetPath;
	FString AssetName;
	FString PackageName;


	UBlueprint* BlueprintAsset;
	
	UBlueprintFactoryBase* BlueprintFactory;

	EAppReturnType::Type UserResponse;

	bool bAutoOpenAsset;

public:
	/** Gets the resulting asset path */
	FString GetAssetPath() const { return AssetPath; }

	/** Gets the resulting asset name */
	FString GetAssetName() const { return AssetName; }

	/** Gets the resulting package name */
	FString GetPackageName() const { return PackageName; }

	/** Gets the BlueprintAsset */
	UBlueprint* GetBlueprintAsset() const { return BlueprintAsset; }
};
