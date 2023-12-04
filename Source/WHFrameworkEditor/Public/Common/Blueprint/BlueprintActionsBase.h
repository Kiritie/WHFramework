// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_Blueprint.h"

class UBlueprintFactoryBase;
class FBlueprintEditorBase;
class UFactory;

class FBlueprintActionsBase : public FAssetTypeActions_Blueprint
{
public:
	FBlueprintActionsBase(EAssetTypeCategories::Type InAssetCategory);

public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return AssetName; }
	virtual FColor GetTypeColor() const override { return AssetColor; }
	virtual UClass* GetSupportedClass() const override { return AssetClass; }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual uint32 GetCategories() override { return AssetCategory; }
	// End IAssetTypeActions Implementation

	virtual TSharedRef<FBlueprintEditorBase> CreateBlueprintEditor() const;

	// FAssetTypeActions_Blueprint interface
	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const override;

private:
	/** Returns true if the blueprint is data only */
	bool ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const;

public:
	FText AssetName;

	FColor AssetColor;

	UClass* AssetClass;
	
	UClass* FactoryClass;

	EAssetTypeCategories::Type AssetCategory;
};
