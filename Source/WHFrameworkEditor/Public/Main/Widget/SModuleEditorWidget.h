// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/Base/SMainEditorWidgetBase.h"

class AMainModule;

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SModuleEditorWidget : public SMainEditorWidgetBase
{
public:
	SModuleEditorWidget();
	
	SLATE_BEGIN_ARGS(SModuleEditorWidget) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& InNomadTab);

public:
	virtual void OnCreate() override;

	virtual void OnSave() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

protected:
	virtual void OnBindCommands(const TSharedRef<FUICommandList>& InCommands) override;

protected:
	virtual void RegisterMenuBar(FMenuBarBuilder& InMenuBarBuilder) override;

	virtual void RegisterTabSpawners() override;

	virtual void UnRegisterTabSpawners() override;

	virtual TSharedRef<FTabManager::FLayout> CreateDefaultLayout() override;

	virtual TSharedRef<SWidget> CreateMainWidget() override;

protected:
	void OnBeginPIE(bool bIsSimulating);

	void OnEndPIE(bool bIsSimulating);

	void OnMapOpened(const FString& Filename, bool bAsTemplate);

	void OnMapChanged(uint32 MapChangeFlags);

	void OnBlueprintCompiled();

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

	bool bNeedRebuild;

	AMainModule* MainModule;

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

public:
	void TogglePreview();

	void SetIsPreview(bool bIsPreview);
};
