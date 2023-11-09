// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/SEditorWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SProcedureEditorWidget : public SEditorWidgetBase
{
public:
	SProcedureEditorWidget();
	
	SLATE_BEGIN_ARGS(SProcedureEditorWidget) {}
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
	bool bPreviewMode;
	bool bShowListPanel;
	bool bShowDetailsPanel;
	bool bShowStatusPanel;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	class UProcedureModule* ProcedureModule;
	
	TSharedPtr<FTabManager> TabManager;
	TMap<FName, TWeakPtr<SDockTab>> SpawnedTabs;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<class SProcedureToolbarWidget> ToolbarWidget;
	
	TSharedPtr<class SProcedureListWidget> ListWidget;

	TSharedPtr<class SProcedureDetailsWidget> DetailsWidget;

	TSharedPtr<class SProcedureStatusWidget> StatusWidget;

private:
	FDelegateHandle OnBeginPIEHandle;
	FDelegateHandle OnEndPIEHandle;
	FDelegateHandle OnMapOpenedHandle;
	FDelegateHandle OnBlueprintCompiledHandle;

public:
	void TogglePreviewMode();

	void SetIsPreviewMode(bool bIsPreviewMode);
};
