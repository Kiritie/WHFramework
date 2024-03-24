// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "ClassViewerFilter.h"
#include "Common/CommonStatics.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Base/SEditorWidgetBase.h"

#define GENERATED_EDITOR_MODULE(ModuleClass) \
protected: \
	static ModuleClass* Instance; \
public: \
	static ModuleClass& Get();

#define IMPLEMENTATION_EDITOR_MODULE(ModuleClass) \
ModuleClass* ModuleClass::Instance = nullptr; \
ModuleClass& ModuleClass::Get() \
{ \
	if(!Instance) \
	{ \
		Instance = new ModuleClass(); \
	} \
	return *Instance; \
}

class FClassViewerFilterBase : public IClassViewerFilter
{
public:
	FClassViewerFilterBase()
		: bIncludeBaseClasses(true), bIncludeParentClasses(true), bDisallowBlueprintBase(false), DisallowedClassFlags(CLASS_None)
	{
	}

	/** All children of these classes will be included unless filtered out by another setting. */
	TSet< const UClass* > AllowedChildrenOfClasses;

	/** Include parent classes. */
	bool bIncludeBaseClasses;

	/** Include parent classes. */
	bool bIncludeParentClasses;

	/** Disallow blueprint base classes. */
	bool bDisallowBlueprintBase;

	/** Disallowed class flags. */
	EClassFlags DisallowedClassFlags;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		const bool bAllowed = !InClass->HasAnyClassFlags(DisallowedClassFlags)
			&& InClass->CanCreateAssetOfClass()
			&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed
			&& (bIncludeBaseClasses || InFilterFuncs->IfInClassesSet(AllowedChildrenOfClasses, InClass) == EFilterReturn::Failed)
			&& (bIncludeParentClasses || !UCommonStatics::IsClassHasChildren(InClass))
			&& IsClassAllowed(const_cast<UClass*>(InClass));

		if (bAllowed && bDisallowBlueprintBase)
		{
			if (FKismetEditorUtilities::CanCreateBlueprintOfClass(InClass))
			{
				return false;
			}
		}

		return bAllowed;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		if (bDisallowBlueprintBase)
		{
			return false;
		}

		return !InUnloadedClassData->HasAnyClassFlags(DisallowedClassFlags)
			&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed
			&& IsClassAllowed(InUnloadedClassData);
	}
		
	virtual bool IsClassAllowed(UClass* InClass)
	{
		return true;
	}

	virtual bool IsClassAllowed(const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData)
	{
		return IsClassAllowed(StaticLoadClass(UObject::StaticClass(), nullptr, *InUnloadedClassData->GetClassPathName().ToString()));
	}
};
