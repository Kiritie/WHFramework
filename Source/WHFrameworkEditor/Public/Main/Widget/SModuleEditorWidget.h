// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/Base/SEditorWidgetBase.h"

class AMainModule;

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SModuleEditorWidget : public SEditorWidgetBase
{
public:
	SModuleEditorWidget();
	
	SLATE_BEGIN_ARGS(SModuleEditorWidget) {}
	
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	virtual void OnCreate() override;

	virtual void OnSave() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

protected:
	virtual void OnBindCommands(const TSharedRef<FUICommandList>& InCommands) override;

protected:
	void OnBeginPIE(bool bIsSimulating);

	void OnEndPIE(bool bIsSimulating);

	void OnMapOpened(const FString& Filename, bool bAsTemplate);

	void OnMapChanged(uint32 MapChangeFlags);

	void OnBlueprintCompiled();
	
	void OnTabSpawned(const FName& TabIdentifier, const TSharedRef<SDockTab>& SpawnedTab);

	void HandleTabManagerPersistLayout(const TSharedRef<FTabManager::FLayout>& LayoutToSave);

	TSharedRef<SDockTab> SpawnToolbarWidgetTab(const FSpawnTabArgs& Args);
	
	TSharedRef<SDockTab> SpawnListWidgetTab(const FSpawnTabArgs& Args);

	TSharedRef<SDockTab> SpawnDetailsWidgetTab(const FSpawnTabArgs& Args);

	TSharedRef<SDockTab> SpawnStatusWidgetTab(const FSpawnTabArgs& Args);
		
	void HandlePullDownFileMenu(FMenuBuilder& MenuBuilder);

	void HandlePullDownWindowMenu(FMenuBuilder& MenuBuilder);

	FReply OnCreateMainModuleButtonClicked();

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	bool bPreview;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	AMainModule* MainModule;
	
	TSharedPtr<FTabManager> TabManager;
	
	TMap<FName, TWeakPtr<SDockTab>> SpawnedTabs;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<class SModuleToolbarWidget> ToolbarWidget;
	
	TSharedPtr<class SModuleListWidget> ListWidget;

	TSharedPtr<class SModuleDetailsWidget> DetailsWidget;

	TSharedPtr<class SModuleStatusWidget> StatusWidget;

private:
	FDelegateHandle OnBeginPIEHandle;
	FDelegateHandle OnEndPIEHandle;
	FDelegateHandle OnMapChangeHandle;
	FDelegateHandle OnMapOpenedHandle;
	FDelegateHandle OnBlueprintCompiledHandle;

	bool bNeedRebuild;

public:
	void TogglePreview();

	void SetIsPreview(bool bIsPreview);
};
