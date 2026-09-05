#include "Dialogue/Style/DialogueEditorStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"

#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )

FString FDialogueEditorStyle::RootToContentDir(const ANSICHAR* RelativePath, const TCHAR* Extension)
{
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("WHFramework"))->GetBaseDir() / TEXT("Resources/Dialogue");
	return (ContentDir / RelativePath) + Extension;
}

TSharedPtr< FSlateStyleSet > FDialogueEditorStyle::StyleSet = nullptr;
TSharedPtr< ISlateStyle > FDialogueEditorStyle::Get() { return StyleSet; }

FName FDialogueEditorStyle::GetStyleSetName()
{
	static FName DialogueEditorStyleName(TEXT("DialogueEditorStyle"));
	return DialogueEditorStyleName;
}

void FDialogueEditorStyle::Initialize()
{
	 //Only register once
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	{
		StyleSet->Set("PlayerNodeStyle", new BOX_BRUSH("playerNode", FMargin(5.f / 138.f, 5.f / 56.f, 5.f / 138.f, 5.f / 56.f))); // left top right bottom
		StyleSet->Set("NpcNodeStyle", new BOX_BRUSH("npcNode", FMargin(5.f / 138.f, 5.f / 56.f, 5.f / 138.f, 5.f / 56.f)));
		StyleSet->Set("StartNodeStyle", new BOX_BRUSH("startNode", FMargin(5.f / 138.f, 5.f / 56.f, 5.f / 138.f, 5.f / 56.f)));

		StyleSet->Set("EventIcon", new IMAGE_BRUSH("event", FVector2D(14, 16)));
		StyleSet->Set("ConditionIcon", new IMAGE_BRUSH("condition", FVector2D(16, 16)));
		StyleSet->Set("VoiceIcon", new IMAGE_BRUSH("sound", FVector2D(17, 16)));

		StyleSet->Set(FName(TEXT("ClassThumbnail.DialogueAsset")), new IMAGE_BRUSH("dialogue_64", FVector2D(64, 64)));
		StyleSet->Set(FName(TEXT("ClassIcon.DialogueAsset")), new IMAGE_BRUSH("dialogue_16", FVector2D(16, 16)));
	}

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
};

#undef BOX_BRUSH
#undef IMAGE_BRUSH


void FDialogueEditorStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}
