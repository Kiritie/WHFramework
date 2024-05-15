// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Slate/Editor/Base/SEditorWidgetBase.h"


/**
 * 
 */
class WHFRAMEWORKSLATE_API SMainEditorWidgetBase : public SEditorWidgetBase
{
public:
	SMainEditorWidgetBase();
	
	SLATE_BEGIN_ARGS(SMainEditorWidgetBase) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& InNomadTab);

public:
	static FName WidgetName;

public:
	virtual void OnCreate() override;

	virtual void OnSave() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

protected:
	virtual void OnBindCommands(const TSharedRef<FUICommandList>& InCommands) override;

protected:
	virtual void RegisterMenuBar(FMenuBarBuilder& InMenuBarBuilder);

	virtual void RegisterTabSpawners();

	virtual void UnRegisterTabSpawners();

	virtual TSharedRef<FTabManager::FLayout> CreateDefaultLayout();

	virtual TSharedRef<SWidget> CreateMainWidget();

	virtual TSharedRef<SWidget> CreateMenuBarWidget();

	virtual TSharedRef<SWidget> CreateDefaultWidget();

protected:
	void OnTabSpawned(const FName& TabIdentifier, const TSharedRef<SDockTab>& SpawnedTab);

	void OnPersistLayout(const TSharedRef<FTabManager::FLayout>& LayoutToSave);
	
	FTabSpawnerEntry& RegisterTabSpawner(const FName& TabId, const FOnSpawnTab& OnSpawnTab);;
	
	void UnRegisterTabSpawner(const FName& TabId);;

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	FName DefaultLayoutName;
	
	TSharedPtr<FTabManager> TabManager;
	
	TMap<FName, TWeakPtr<SDockTab>> SpawnedTabs;
};
