// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/Asset/StepAssetFactory.h"

#include "Step/Base/StepAsset.h"

#define LOCTEXT_NAMESPACE "StepAssetFactory"

UStepAssetFactory::UStepAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UStepAsset::StaticClass();
	ParentClass = UStepAsset::StaticClass();
}

#undef LOCTEXT_NAMESPACE // "StepAssetFactory"
