// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/Asset/TaskAssetFactory.h"

#include "Task/Base/TaskAsset.h"

#define LOCTEXT_NAMESPACE "TaskAssetFactory"

UTaskAssetFactory::UTaskAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UTaskAsset::StaticClass();
	ParentClass = UTaskAsset::StaticClass();
}

#undef LOCTEXT_NAMESPACE // "TaskAssetFactory"
