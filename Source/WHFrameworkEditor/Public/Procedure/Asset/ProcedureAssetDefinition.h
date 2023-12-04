// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/Asset/AssetDefinitionBase.h"

#include "ProcedureAssetDefinition.generated.h"

UCLASS()
class UProcedureAssetDefinition : public UAssetDefinitionBase
{
	GENERATED_BODY()

public:
	UProcedureAssetDefinition();
	
public:
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
