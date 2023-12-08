// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "ClassViewerFilter.h"
#include "Common/CommonStatics.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Widget/Base/SEditorWidgetBase.h"

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

#define SNewEd( WidgetType, ... ) \
MakeTDeclEd<WidgetType>( #WidgetType, nullptr, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__) ) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()

#define SNewEdN( WidgetType, ParentWidget, ... ) \
MakeTDeclEd<WidgetType>( #WidgetType, ParentWidget, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__) ) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()

#define SAssignNewEd( ExposeAs, WidgetType, ... ) \
MakeTDeclEd<WidgetType>( #WidgetType, nullptr, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__) ) . Expose( ExposeAs ) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()

#define SAssignNewEdN( ExposeAs, WidgetType, ParentWidget, ... ) \
MakeTDeclEd<WidgetType>( #WidgetType, ParentWidget, __FILE__, __LINE__, RequiredArgs::MakeRequiredArgs(__VA_ARGS__) ) . Expose( ExposeAs ) <<= TYPENAME_OUTSIDE_TEMPLATE WidgetType::FArguments()

template<typename WidgetType, typename RequiredArgsPayloadType>
TSlateDecl<WidgetType, RequiredArgsPayloadType> MakeTDeclEd( const ANSICHAR* InType, const TSharedPtr<SEditorWidgetBase>& InParentWidget, const ANSICHAR* InFile, int32 OnLine, RequiredArgsPayloadType&& InRequiredArgs )
{
	LLM_SCOPE_BYTAG(UI_Slate);
	TSlateDecl<WidgetType, RequiredArgsPayloadType> SlateDecl(InType, InFile, OnLine, Forward<RequiredArgsPayloadType>(InRequiredArgs));
	if(InParentWidget)
	{
		InParentWidget->AddChild(SlateDecl._Widget);
	}
	return SlateDecl;
}
