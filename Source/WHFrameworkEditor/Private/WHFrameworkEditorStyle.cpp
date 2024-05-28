// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFrameworkEditorStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define IMAGE_BRUSH_SVG(RelativePath, ...) FSlateVectorImageBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)

TSharedPtr<FSlateStyleSet> FWHFrameworkEditorStyle::StyleInstance = nullptr;

void FWHFrameworkEditorStyle::Initialize()
{
	if(!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FWHFrameworkEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FWHFrameworkEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("WHFrameworkEditorStyle"));
	return StyleSetName;
}

TSharedRef<FSlateStyleSet> FWHFrameworkEditorStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("WHFrameworkEditorStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("WHFramework")->GetBaseDir() / TEXT("Resources"));
	
	// Note, these sizes are in Slate Units.
	// Slate Units do NOT have to map to pixels.
	const FVector2D Icon5x16(5.0f, 16.0f);
	const FVector2D Icon8x4(8.0f, 4.0f);
	const FVector2D Icon8x8(8.0f, 8.0f);
	const FVector2D Icon10x10(10.0f, 10.0f);
	const FVector2D Icon12x12(12.0f, 12.0f);
	const FVector2D Icon12x16(12.0f, 16.0f);
	const FVector2D Icon14x14(14.0f, 14.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon22x22(22.0f, 22.0f);
	const FVector2D Icon24x24(24.0f, 24.0f);
	const FVector2D Icon25x25(25.0f, 25.0f);
	const FVector2D Icon32x32(32.0f, 32.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon64x64(64.0f, 64.0f);
	const FVector2D Icon36x24(36.0f, 24.0f);
	const FVector2D Icon128x128(128.0f, 128.0f);

	Style->Set("ModuleEditor.OpenModuleEditorWindow", new IMAGE_BRUSH(TEXT("Icon_ModuleEditor_40"), Icon40x40));

	Style->Set("ProcedureEditor.OpenProcedureEditorWindow", new IMAGE_BRUSH(TEXT("Icon_ProcedureEditor_40"), Icon40x40));

	Style->Set("StepEditor.OpenStepEditorWindow", new IMAGE_BRUSH(TEXT("Icon_StepEditor_40"), Icon40x40));

	Style->Set("TaskEditor.OpenTaskEditorWindow", new IMAGE_BRUSH(TEXT("Icon_TaskEditor_40"), Icon40x40));

	Style->Set("AssetModifierEditor.OpenAssetModifierEditorWindow", new IMAGE_BRUSH_SVG(TEXT("Icon_AssetModifierEditor_40"), Icon40x40));

	return Style;
}

void FWHFrameworkEditorStyle::ReloadTextures()
{
	if(FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FWHFrameworkEditorStyle::Get()
{
	return *StyleInstance;
}

#undef IMAGE_BRUSH
#undef IMAGE_BRUSH_SVG
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef DEFAULT_FONT
