// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/Asset/AssetFactoryBase.h"
#include "UObject/ObjectMacros.h"
#include "ProcedureAssetFactory.generated.h"

UCLASS(hidecategories=Object, MinimalAPI)
class UProcedureAssetFactory : public UAssetFactoryBase
{
	GENERATED_BODY()

public:
	UProcedureAssetFactory(const FObjectInitializer& ObjectInitializer);
};
