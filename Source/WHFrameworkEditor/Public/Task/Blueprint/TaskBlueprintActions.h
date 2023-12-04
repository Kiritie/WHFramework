// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/Blueprint/BlueprintActionsBase.h"

class FTaskBlueprintActions : public FBlueprintActionsBase
{
public:
	FTaskBlueprintActions(EAssetTypeCategories::Type InAssetCategory);

public:
	virtual TSharedRef<FBlueprintEditorBase> CreateBlueprintEditor() const override;
};
