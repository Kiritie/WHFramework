// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FEditorModuleBase;

//////////////////////////////////////////////////////////////////////////
/// AssetEditorBase
class FAssetEditorBase : public FAssetEditorToolkit, public FEditorUndoClient
{
public:
	FAssetEditorBase();

	~FAssetEditorBase();

public:
	virtual void PostInitAssetEditor() override;
	
	virtual void InitAssetEditorBase( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* Asset );

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	
	virtual FTabSpawnerEntry& RegisterTrackedTabSpawner(const TSharedRef<class FTabManager>& InTabManager, const FName& TabId, const FOnSpawnTab& OnSpawnTab);;

	virtual TSharedRef<FTabManager::FLayout> CreateDefaultLayout();
	
	virtual void ExtendToolbar(FToolBarBuilder& ToolbarBuilder);

	virtual void PostRegenerateMenusAndToolbars() override;
	
	/** IToolkit interface */
	virtual FName GetToolkitFName() const override { return ToolkitFName; }
	virtual FText GetBaseToolkitName() const override { return BaseToolkitName; }
	virtual FString GetWorldCentricTabPrefix() const override { return WorldCentricTabPrefix; }
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return WorldCentricTabColorScale; }

	// FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

protected:
	virtual void OnBlueprintCompiled();

	//////////////////////////////////////////////////////////////////////////
	/// Stats
protected:
	UObject* EditingAsset;

public:
	template<class T>
	T* GetEditingAsset() const
	{
		return Cast<T>(EditingAsset);
	}

	UObject* GetEditingAsset() const { return EditingAsset; }

	virtual FEditorModuleBase* GetEditorModule() const;

public:
	FName ToolkitFName;
	FText BaseToolkitName;

	FText MenuCategory;
	FName DefaultLayoutName;
	
	FString WorldCentricTabPrefix;
	FLinearColor WorldCentricTabColorScale;
		
	FDelegateHandle OnBlueprintCompiledHandle;
};
