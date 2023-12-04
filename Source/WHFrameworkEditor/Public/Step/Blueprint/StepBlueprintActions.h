// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/Blueprint/BlueprintActionsBase.h"

class FStepBlueprintActions : public FBlueprintActionsBase
{
public:
	FStepBlueprintActions(EAssetTypeCategories::Type InAssetCategory);

public:
	virtual TSharedRef<FBlueprintEditorBase> CreateBlueprintEditor() const override;
};
