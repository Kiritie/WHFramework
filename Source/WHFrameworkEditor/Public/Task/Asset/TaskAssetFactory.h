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
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
};
