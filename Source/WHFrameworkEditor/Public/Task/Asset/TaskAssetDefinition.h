// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/Asset/AssetDefinitionBase.h"

#include "TaskAssetDefinition.generated.h"

UCLASS()
class UTaskAssetDefinition : public UAssetDefinitionBase
{
	GENERATED_BODY()

public:
	UTaskAssetDefinition();
	
public:
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
