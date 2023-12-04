// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/Blueprint/BlueprintActionsBase.h"

class FModuleBlueprintActions : public FBlueprintActionsBase
{
public:
	FModuleBlueprintActions(EAssetTypeCategories::Type InAssetCategory);

public:
	virtual TSharedRef<FBlueprintEditorBase> CreateBlueprintEditor() const override;
};
