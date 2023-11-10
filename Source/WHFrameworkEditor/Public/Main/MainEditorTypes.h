// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "ClassViewerFilter.h"
#include "Common/CommonStatics.h"
#include "Main/MainModule.h"

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

FString GModuleEditorIni;

class FModuleClassFilter : public IClassViewerFilter
{
public:
	const UClass* IncludeParentClass;
	const UClass* UnIncludeParentClass;
	
	AMainModule* MainModule;

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
		return InClass != IncludeParentClass && InClass->IsChildOf(IncludeParentClass) && !InClass->IsChildOf(UnIncludeParentClass) && !MainModule->GetModuleMap().Contains(InClass->GetDefaultObject<UModuleBase>()->GetModuleName()) && UCommonStatics::GetClassChildren(InClass).IsEmpty();
	}
};
