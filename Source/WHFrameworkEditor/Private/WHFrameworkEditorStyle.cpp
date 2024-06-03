// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFrameworkEditorStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleMacros.h"

using namespace CoreStyleConstants;

TSharedPtr<ISlateStyle> FWHFrameworkEditorStyle::Instance = nullptr;

void FWHFrameworkEditorStyle::Initialize()
{
	if(!Instance.IsValid())
	{
		Instance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*Instance);
	}
}

void FWHFrameworkEditorStyle::Shutdown()
{
	if(Instance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*Instance);
		Instance.Reset();
	}
}

const ISlateStyle& FWHFrameworkEditorStyle::Get()
{
	return *Instance;
}

FName FWHFrameworkEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("WHFrameworkEditorStyle"));
	return StyleSetName;
}

TSharedRef<ISlateStyle> FWHFrameworkEditorStyle::Create()
{
	TSharedRef<class FWHFrameworkEditorStyle::FStyle> Style = MakeShareable(new FWHFrameworkEditorStyle::FStyle());
	Style->Initialize();
	return Style;
}

FWHFrameworkEditorStyle::FStyle::FStyle()
	: FSlateStyleSet(FWHFrameworkEditorStyle::GetStyleSetName())
{
}

FWHFrameworkEditorStyle::FStyle::~FStyle()
{
}

void FWHFrameworkEditorStyle::FStyle::Initialize()
{
	SetContentRoot(IPluginManager::Get().FindPlugin("WHFramework")->GetBaseDir() / TEXT("Resources"));

	Set("ModuleEditor.OpenModuleEditorWindow", new IMAGE_BRUSH(TEXT("Icon_ModuleEditor_40"), Icon40x40));

	Set("ProcedureEditor.OpenProcedureEditorWindow", new IMAGE_BRUSH(TEXT("Icon_ProcedureEditor_40"), Icon40x40));

	Set("StepEditor.OpenStepEditorWindow", new IMAGE_BRUSH(TEXT("Icon_StepEditor_40"), Icon40x40));

	Set("TaskEditor.OpenTaskEditorWindow", new IMAGE_BRUSH(TEXT("Icon_TaskEditor_40"), Icon40x40));

	Set("AssetModifierEditor.OpenAssetModifierEditorWindow", new IMAGE_BRUSH_SVG(TEXT("Icon_AssetModifierEditor_40"), Icon40x40));
}
