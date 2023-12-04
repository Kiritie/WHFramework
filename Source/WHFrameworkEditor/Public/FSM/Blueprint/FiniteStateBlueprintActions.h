// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/Blueprint/BlueprintActionsBase.h"

class FFiniteStateBlueprintActions : public FBlueprintActionsBase
{
public:
	FFiniteStateBlueprintActions(EAssetTypeCategories::Type InAssetCategory);

public:
	virtual TSharedRef<FBlueprintEditorBase> CreateBlueprintEditor() const override;
};
