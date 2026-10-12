// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/Asset/StepAssetFactory.h"

#include "Step/Base/StepAsset.h"

#define LOCTEXT_NAMESPACE "StepAssetFactory"

UStepAssetFactory::UStepAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UStepAsset::StaticClass();
	ParentClass = UStepAsset::StaticClass();
	PickedClass = UStepAsset::StaticClass();
}

UObject* UStepAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if(!Class || !Class->IsChildOf(UStepAsset::StaticClass()) || !InParent)
	{
		return nullptr;
	}
	UClass* AssetClass = PickedClass ? PickedClass.Get() : Class;
	if(AssetClass == UStepAsset::StaticClass())
	{
		AssetClass = Class;
	}
	if(!AssetClass->IsChildOf(UStepAsset::StaticClass()) || AssetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return nullptr;
	}
	UStepAsset* Asset = NewObject<UStepAsset>(InParent, AssetClass, Name, Flags | RF_Transactional);
	Asset->StartGUID = FGuid::NewGuid().ToString();
	Asset->StartPosition = FVector2D::ZeroVector;
	Asset->Steps.Reset();
	Asset->Links.Reset();
	TArray<FText> Errors;
	if(!Asset->Compile(Errors))
	{
		Asset->ClearFlags(RF_Public | RF_Standalone);
		Asset->MarkAsGarbage();
		return nullptr;
	}
	return Asset;
}

UObject* UStepAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn);
}

#undef LOCTEXT_NAMESPACE // "StepAssetFactory"
