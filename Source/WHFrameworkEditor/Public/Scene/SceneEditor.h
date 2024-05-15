// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WHFrameworkEditorTypes.h"
#include "Main/Module/EditorModuleBase.h"
#include "Scene/SceneManager.h"
#include "Slate/SViewportRootCanvas.h"

class ILevelEditor;
class SConstraintCanvas;

//////////////////////////////////////////////////////////////////////////
// SceneEditorModule
class FSceneEditorModule : public FEditorModuleBase
{
	GENERATED_EDITOR_MODULE(FSceneEditorModule)

public:
	FSceneEditorModule();
	
public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

public:
	virtual void RegisterSettings(ISettingsModule* SettingsModule) override;

	virtual void UnRegisterSettings(ISettingsModule* SettingsModule) override;

	virtual bool HandleSettingsSaved() override;
	
	virtual void RegisterCommands(const TSharedPtr<FUICommandList>& Commands) override;

	virtual void RegisterMenus(const TSharedPtr<FUICommandList>& Commands) override;

	virtual void RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions) override;

	virtual void RegisterCustomization(FPropertyEditorModule& PropertyEditor) override;

	virtual void UnRegisterCustomization(FPropertyEditorModule& PropertyEditor) override;
};

//////////////////////////////////////////////////////////////////////////
// SceneEditorManager
class WHFRAMEWORKEDITOR_API FSceneEditorManager : public FSceneManager
{
	GENERATED_MANAGER(FSceneEditorManager)

public:
	// ParamSets default values for this actor's properties
	FSceneEditorManager();

	virtual ~FSceneEditorManager() override;

	static const FUniqueType Type;

public:
	virtual void OnInitialize() override;

protected:
	void OnLevelEditorCreated(TSharedPtr<ILevelEditor> InLevelEditor);

	void OnActiveViewportChanged(TSharedPtr<IAssetViewport> OldViewport, TSharedPtr<IAssetViewport> NewViewport);

	//////////////////////////////////////////////////////////////////////////
	// SceneView
public:
	bool GetSceneViewProjectionData(const FEditorViewportClient* InViewport, FSceneViewProjectionData& OutProjectionData);

	//////////////////////////////////////////////////////////////////////////
	// World
public:
	virtual void SelectActorsInCurrentWorld(const TArray<AActor*>& InActors, bool bForce) override;

	virtual void DeselectActorsInCurrentWorld(const TArray<AActor*>& InActors) override;

	virtual void DeselectAllActorInCurrentWorld() override;

	virtual TArray<AActor*> GetSelectedActorsInCurrentWorld() const override;

	//////////////////////////////////////////////////////////////////////////
	// Viewport
public:
	virtual SConstraintCanvas::FSlot* AddWidgetToViewport(const TSharedPtr<IAssetViewport>& InViewport, const TSharedRef<SWidget>& InWidget);
	
	virtual SConstraintCanvas::FSlot* AddWidgetToActiveViewport(const TSharedRef<SWidget>& InWidget) override;

	virtual TArray<SConstraintCanvas::FSlot*> AddWidgetToAllViewport(const TSharedRef<SWidget>& InWidget) override;

	virtual void RemoveWidgetFromViewport(const TSharedPtr<IAssetViewport>& InViewport, const TSharedRef<SWidget>& InWidget);

	virtual void RemoveWidgetFromActiveViewport(const TSharedRef<SWidget>& InWidget) override;

	virtual void RemoveWidgetFromAllViewport(const TSharedRef<SWidget>& InWidget) override;

	TSharedRef<SViewportRootCanvas> GetOrCreateViewportRootCanvas(const TSharedPtr<IAssetViewport>& InViewport);

protected:
	TMap<TSharedPtr<IAssetViewport>, TSharedPtr<SViewportRootCanvas>> ViewportRootCanvases;
};
