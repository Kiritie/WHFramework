// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerFilter.h"

FString GStepEditorIni;

class FStepClassFilter : public IClassViewerFilter
{
public:
	const UClass* IncludeParentClass;
	const UClass* UnIncludeParentClass;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		return IsClassAllowedHelper(InClass);
	}
	
	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InBlueprint, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return IsClassAllowedHelper(InBlueprint);
	}

private:
	template <typename TClass>
	bool IsClassAllowedHelper(TClass InClass)
	{
		return InClass->IsChildOf(IncludeParentClass) && !InClass->IsChildOf(UnIncludeParentClass);
	}
};