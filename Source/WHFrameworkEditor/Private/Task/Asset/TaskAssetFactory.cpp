// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/Asset/TaskAssetFactory.h"

#include "Task/Base/TaskAsset.h"

#define LOCTEXT_NAMESPACE "TaskAssetFactory"

UTaskAssetFactory::UTaskAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UTaskAsset::StaticClass();
	ParentClass = UTaskAsset::StaticClass();
	PickedClass = UTaskAsset::StaticClass();
}

UObject* UTaskAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UTaskAsset* Asset = Cast<UTaskAsset>(Super::FactoryCreateNew(Class, InParent, Name, Flags | RF_Transactional, Context, Warn));
	if(Asset)
	{
		Asset->InitializeGraph();
	}
	return Asset;
}

UObject* UTaskAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn);
}

#undef LOCTEXT_NAMESPACE // "TaskAssetFactory"
