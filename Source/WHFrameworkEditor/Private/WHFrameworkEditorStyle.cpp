// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFrameworkEditorStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FWHFrameworkEditorStyle::StyleInstance = NULL;

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

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )

const FVector2D Icon8x8(8.0f, 8.0f);
const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

TSharedRef<FSlateStyleSet> FWHFrameworkEditorStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("WHFrameworkEditorStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("WHFramework")->GetBaseDir() / TEXT("Resources"));

	Style->Set("ModuleEditor.OpenModuleEditorWindow", new IMAGE_BRUSH(TEXT("Icon_ModuleEditor_40"), Icon40x40));

	Style->Set("ProcedureEditor.OpenProcedureEditorWindow", new IMAGE_BRUSH(TEXT("Icon_ProcedureEditor_40"), Icon40x40));

	Style->Set("StepEditor.OpenStepEditorWindow", new IMAGE_BRUSH(TEXT("Icon_StepEditor_40"), Icon40x40));

	Style->Set("TaskEditor.OpenTaskEditorWindow", new IMAGE_BRUSH(TEXT("Icon_TaskEditor_40"), Icon40x40));

	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT

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
