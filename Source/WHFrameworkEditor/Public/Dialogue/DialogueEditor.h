#pragma once

#include "WHFrameworkEditorTypes.h"
#include "Common/Asset/AssetEditorBase.h"
#include "Main/Module/EditorModuleBase.h"
#include "Dialogue/DialogueEditorTypes.h"
#include "Dialogue/Base/DialogueAsset.h"

class FDialogueEditor;
class SDialogueViewportWidget;

class FDialogueEditorModule : public FEditorModuleBase
{
	GENERATED_EDITOR_MODULE(FDialogueEditorModule)
public:
	FDialogueEditorModule();
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual void RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions) override;
	TSharedRef<FDialogueEditor> CreateDialogueEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& Host, UDialogueAsset* Asset);
	static uint32 GameAssetCategory;
};

class FDialogueEditor : public FAssetEditorBase
{
public:
	FDialogueEditor();
	virtual void InitAssetEditorBase(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& Host, UObject* Asset) override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual TSharedRef<FTabManager::FLayout> CreateDefaultLayout() override;
	virtual FEditorModuleBase* GetEditorModule() const override;
	virtual void ExtendToolbar(FToolBarBuilder& ToolbarBuilder) override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	virtual bool ProcessCommandBindings(const FKeyEvent& InKeyEvent) const override;
	UDialogueAsset* GetDialogueBeingEdited() const { return GetEditingAsset<UDialogueAsset>(); }
	TSharedRef<FDialogueEditorState> GetEditingState() const { return EditingState; }

	TSharedPtr<SDialogueViewportWidget> DialogueViewportWidget;
	bool refreshDetails = false;

protected:
	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);

private:
	void BindCommands();
	void OnValidateDialogue();
	TSharedRef<FDialogueEditorState> EditingState;
};
