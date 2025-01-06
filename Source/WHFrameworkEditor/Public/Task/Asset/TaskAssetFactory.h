// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/Asset/AssetFactoryBase.h"
#include "UObject/ObjectMacros.h"
#include "TaskAssetFactory.generated.h"

UCLASS(hidecategories=Object, MinimalAPI)
class UTaskAssetFactory : public UAssetFactoryBase
{
	GENERATED_BODY()

public:
	UTaskAssetFactory(const FObjectInitializer& ObjectInitializer);
};
