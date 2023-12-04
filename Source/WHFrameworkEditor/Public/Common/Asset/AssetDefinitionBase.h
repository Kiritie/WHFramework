// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"

#include "AssetDefinitionBase.generated.h"

UCLASS(Abstract, MinimalAPI)
class UAssetDefinitionBase : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	UAssetDefinitionBase();

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return AssetDisplayName; }
	virtual FLinearColor GetAssetColor() const override { return AssetColor; }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return AssetClass; }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override { return AssetCategories; }

	virtual bool CanImport() const override { return true; }
	// UAssetDefinition End

public:
	UPROPERTY(EditAnywhere)
	FText AssetDisplayName;

	TArray<FAssetCategoryPath> AssetCategories;

	UPROPERTY(EditAnywhere)
	FLinearColor AssetColor;

	UPROPERTY(EditAnywhere)
	TSoftClassPtr<UObject> AssetClass;
};
