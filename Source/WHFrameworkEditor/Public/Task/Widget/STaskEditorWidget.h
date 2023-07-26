// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/SEditorWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API STaskEditorWidget : public SEditorWidgetBase
{
public:
	STaskEditorWidget();
	
	SLATE_BEGIN_ARGS(STaskEditorWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	virtual void OnCreate() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

protected:
	void OnBeginPIE(bool bIsSimulating);

	void OnEndPIE(bool bIsSimulating);

	void OnMapOpened(const FString& Filename, bool bAsTemplate);

	void OnBlueprintCompiled();
	
	void OnTabSpawned(const FName& TabIdentifier, const TSharedRef<SDockTab>& SpawnedTab);

	void HandleTabManagerPersistLayout(const TSharedRef<FTabManager::FLayout>& LayoutToSave);

	TSharedRef<SDockTab> SpawnToolbarWidgetTab(const FSpawnTabArgs& Args);
	
	TSharedRef<SDockTab> SpawnListWidgetTab(const FSpawnTabArgs& Args);

	TSharedRef<SDockTab> SpawnDetailsWidgetTab(const FSpawnTabArgs& Args);

	TSharedRef<SDockTab> SpawnStatusWidgetTab(const FSpawnTabArgs& Args);
	
	void HandlePullDownWindowMenu(FMenuBuilder& MenuBuilder);

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	bool bPreviewMode = false;
	bool bShowListPanel = true;
	bool bShowDetailPanel = true;
	bool bShowStatusPanel = true;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	class ATaskModule* TaskModule;
	
	TSharedPtr<FTabManager> TabManager;
	TMap<FName, TWeakPtr<SDockTab>> SpawnedTabs;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
protected:
	TSharedPtr<class STaskToolbarWidget> ToolbarWidget;

	TSharedPtr<class STaskListWidget> ListWidget;

	TSharedPtr<class STaskDetailsWidget> DetailsWidget;

	TSharedPtr<class STaskStatusWidget> StatusWidget;

private:
	FDelegateHandle OnBeginPIEHandle;
	FDelegateHandle OnEndPIEHandle;
	FDelegateHandle OnMapOpenedHandle;
	FDelegateHandle OnBlueprintCompiledHandle;

public:
	void TogglePreviewMode();

	void SetIsPreviewMode(bool bIsPreviewMode);
};
