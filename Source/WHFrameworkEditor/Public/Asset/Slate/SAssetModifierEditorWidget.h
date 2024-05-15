// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerModule.h"
#include "ContentBrowserDelegates.h"
#include "Asset/AssetEditorTypes.h"
#include "Slate/Editor/Base/SMainEditorWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SAssetModifierEditorWidget : public SMainEditorWidgetBase
{
public:
	SAssetModifierEditorWidget();
	
	SLATE_BEGIN_ARGS(SAssetModifierEditorWidget) {}

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

	virtual void RegisterMenuBar(FMenuBarBuilder& InMenuBarBuilder) override;

	virtual TSharedRef<SWidget> CreateDefaultWidget() override;

	//////////////////////////////////////////////////////////////////////////
	/// Class Picker
private:
	TSharedRef<SWidget> GenerateClassPicker();

	void OnClassPicked(UClass* InClass);

	FText GetPickedClassName() const;

protected:
	void OnAssetSelected(const FAssetData& AssetData);

	void OnAssetDoubleClicked(const FAssetData& AssetData);
	
	void OnAssetEnterPressed(const TArray<FAssetData>& AssetDatas);

	FReply OnModifyForAllAssetButtonClicked();

	FReply OnModifyForSelectedAssetButtonClicked();

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	TSubclassOf<UAssetModifierBase> SelectedModifierClass;

	TArray<FAssetData> SelectedAssets;

	TArray<FAssetData> ActivatedAssets;

	FGetCurrentSelectionDelegate GetCurrentSelectionDelegate;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<SScrollBar> ScrollBar;

	TSharedPtr<SComboButton> ClassPickButton;

	FClassViewerInitializationOptions ClassViewerOptions;

	TSharedPtr<FAssetModifierClassFilter> ModifierClassFilter;
};
