// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/Asset/AssetDefinitionBase.h"

#include "StepAssetDefinition.generated.h"

UCLASS()
class UStepAssetDefinition : public UAssetDefinitionBase
{
	GENERATED_BODY()

public:
	UStepAssetDefinition();
	
public:
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
