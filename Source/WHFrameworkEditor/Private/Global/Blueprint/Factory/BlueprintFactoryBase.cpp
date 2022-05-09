// Copyright Epic Games, Inc. All Rights Reserved.

#include "Global/Blueprint/Factory/BlueprintFactoryBase.h"

#include "Global/Blueprint/Widget/SCreateBlueprintAssetDialog_ContentBrowser.h"

#define LOCTEXT_NAMESPACE "BlueprintFactoryBase"

UBlueprintFactoryBase::UBlueprintFactoryBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UObject::StaticClass();

	BlueprintType = EBlueprintType::BPTYPE_Normal;
	ParentClass = UObject::StaticClass();
}

bool UBlueprintFactoryBase::ConfigureProperties()
{
	TSharedRef<SCreateBlueprintAssetDialog_ContentBrowser> Dialog = SNew(SCreateBlueprintAssetDialog_ContentBrowser)
		.SupportedClass(ParentClass.Get());
	return Dialog->ConfigureProperties(this);
};

UObject* UBlueprintFactoryBase::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return Super::FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, CallingContext);
}

UObject* UBlueprintFactoryBase::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

#undef LOCTEXT_NAMESPACE
