#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "EditorUndoClient.h"
#include "Misc/NotifyHook.h"
#include "UObject/GCObject.h"
#include "GameplayTagContainer.h"
#include "Widgets/Views/STreeView.h"
#include "Widget/Theme/WidgetStyleTypes.h"
#include "Widget/Theme/WidgetThemeTree.h"

class UWidgetTheme;
class UWidgetThemePreviewButton;
class UCommonTextBlockN;
class UCommonImageN;
class IDetailsView;
class SEditableTextBox;
enum class EWidgetStyleOperation : uint8;

class WHFRAMEWORKEDITOR_API FWidgetThemeToolkit final : public FAssetEditorToolkit, public FGCObject, public FEditorUndoClient, public FNotifyHook
{
public:
	void Open(UWidgetTheme* Theme, const TSharedPtr<IToolkitHost>& Host);
	static TSharedRef<FTabManager::FLayout> CreateDefaultLayout();
	virtual ~FWidgetThemeToolkit() override;
	virtual FName GetToolkitFName() const override
	{
		return TEXT("WidgetThemeEditor");
	}
	virtual FText GetBaseToolkitName() const override
	{
		return NSLOCTEXT("WidgetTheme", "Title", "Widget Theme");
	}
	virtual FString GetWorldCentricTabPrefix() const override
	{
		return TEXT("Widget Theme");
	}
	virtual FLinearColor GetWorldCentricTabColorScale() const override
	{
		return FLinearColor(0.1f, 0.4f, 0.7f);
	}
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FWidgetThemeToolkit");
	}
	virtual void PostUndo(bool Success) override;
	virtual void PostRedo(bool Success) override
	{
		PostUndo(Success);
	}
	virtual void NotifyPreChange(FProperty* Property) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& Event, FProperty* Property) override;
	virtual void SaveAsset_Execute() override;

private:
	TObjectPtr<UWidgetTheme> Theme = nullptr;
	TObjectPtr<UObject> PreviewRoot = nullptr;
	TObjectPtr<UWidgetThemePreviewButton> PreviewButton = nullptr;
	TObjectPtr<UCommonTextBlockN> PreviewText = nullptr;
	TObjectPtr<UCommonImageN> PreviewImage = nullptr;
	TSharedPtr<IDetailsView> Details;
	TSharedPtr<STreeView<TSharedPtr<FThemeTreeItem>>> Tree;
	FVector2D PreviewSize = FVector2D(200, 60);
	bool bPreviewWidthEdited = false;
	bool bPreviewHeightEdited = false;
	bool bShowPreviewText = true;
	TArray<TSharedPtr<FThemeTreeItem>> Roots;
	FGameplayTag SelectedTag;
	FString Search;
	FString Report;
	EWidgetStyleKind TypeFilter = EWidgetStyleKind::None;
	bool bEditingDetails = false;
	bool bCanRemoveSelection = false;
	FDelegateHandle ChangedHandle;
	FDelegateHandle PropertyHandle;
	FDelegateHandle CompileHandle;
	FDelegateHandle DetailsFinishedHandle;
	TSharedRef<SDockTab> SpawnTags(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnPreview(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnContext(const FSpawnTabArgs& Args);
	void ExtendToolbar(FToolBarBuilder& Builder);
	FReply OpenStyleDialog(EWidgetStyleOperation Operation);
	FReply OpenRenameDialog();
	void Refresh();
	void Select(TSharedPtr<FThemeTreeItem> Item, ESelectInfo::Type Type);
	void BindDetails();
	void UpdatePreview();
	void UpdatePreviewSize(UObject* Style, EWidgetStyleKind Kind);
	void FinishDetailsEdit();
	bool CanEditSelection() const;
	FReply Run(const FString& OperationName, TFunction<bool(FString&)> Operation);
};

WHFRAMEWORKEDITOR_API void RegisterWidgetThemeAssetEditor();
WHFRAMEWORKEDITOR_API void UnregisterWidgetThemeAssetEditor();
