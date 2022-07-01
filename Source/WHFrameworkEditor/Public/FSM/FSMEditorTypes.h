// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerFilter.h"
#include "FSM/Components/FSMComponent.h"
#include "Styling/SlateStyle.h"

class FFiniteStateClassFilter : public IClassViewerFilter
{
public:
	const UClass* IncludeParentClass;
	const UClass* UnIncludeParentClass;
	
	UFSMComponent* FSM;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		return IsClassAllowedHelper(InClass);
	}
	
	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InBlueprint, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return IsClassAllowedHelper(InBlueprint->GetClassWithin());
	}

private:
	bool IsClassAllowedHelper(const UClass* InClass)
	{
		return InClass != IncludeParentClass && InClass->IsChildOf(IncludeParentClass) && !InClass->IsChildOf(UnIncludeParentClass) && !FSM->States.Contains(const_cast<UClass*>(InClass));
	}
};