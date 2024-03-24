// Fill out your copyright notice in the Description page of Project Settings.


#include "Base/SMainEditorWidgetBase.h"

#define LOCTEXT_NAMESPACE "SMainEditorWidgetBase"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SMainEditorWidgetBase::SMainEditorWidgetBase()
{
	WidgetName = FName("MainEditorWidgetBase");
	WidgetType = EEditorWidgetType::Main;

	DefaultLayoutName = FName("MainEditorWidgetBase_Layout");
}

void SMainEditorWidgetBase::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& InNomadTab)
{
	InNomadTab->SetOnTabClosed(
	SDockTab::FOnTabClosedCallback::CreateLambda(
		[this](TSharedRef<SDockTab> Self, TWeakPtr<FTabManager> TabManager)
		{
			OnDestroy();
		}
		, TWeakPtr<FTabManager>(TabManager))
	);
	
	TabManager = FGlobalTabmanager::Get()->NewTabManager(InNomadTab);
	TabManager->SetOnPersistLayout(FTabManager::FOnPersistLayout::CreateRaw(this, &SMainEditorWidgetBase::OnPersistLayout));

	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
}

void SMainEditorWidgetBase::OnCreate()
{
	SEditorWidgetBase::OnCreate();
	
	ChildSlot
	[
		CreateMainWidget()
	];
}

void SMainEditorWidgetBase::OnSave()
{
	SEditorWidgetBase::OnSave();
}

void SMainEditorWidgetBase::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SMainEditorWidgetBase::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();
}

void SMainEditorWidgetBase::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();

	UnRegisterTabSpawners();
}

void SMainEditorWidgetBase::OnBindCommands(const TSharedRef<FUICommandList>& InCommands)
{
	SEditorWidgetBase::OnBindCommands(InCommands);
}

void SMainEditorWidgetBase::RegisterMenuBar(FMenuBarBuilder& InMenuBarBuilder)
{
}

void SMainEditorWidgetBase::RegisterTabSpawners()
{
}

void SMainEditorWidgetBase::UnRegisterTabSpawners()
{
}

TSharedRef<FTabManager::FLayout> SMainEditorWidgetBase::CreateDefaultLayout()
{
	const TSharedRef<FTabManager::FLayout> DefaultLayout = FTabManager::NewLayout(DefaultLayoutName);
	return DefaultLayout;
}

TSharedRef<SWidget> SMainEditorWidgetBase::CreateMainWidget()
{
	RegisterTabSpawners();

	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(FLinearColor::Gray) // Darken the outer border
		[
			SNew(SVerticalBox)
	
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				CreateMenuBarWidget()
			]
			
			+ SVerticalBox::Slot()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				CreateTabManagerWidget()
			]
		];
}

TSharedRef<SWidget> SMainEditorWidgetBase::CreateMenuBarWidget()
{
	FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(WidgetCommands);

	RegisterMenuBar(MenuBarBuilder);

	const TSharedRef<SWidget> MenuWidget = MenuBarBuilder.MakeWidget();
	
	TabManager->SetMenuMultiBox(MenuBarBuilder.GetMultiBox(), MenuWidget);

	return MenuWidget;
}

TSharedRef<SWidget> SMainEditorWidgetBase::CreateTabManagerWidget()
{
	return TabManager->RestoreFrom(CreateDefaultLayout(), nullptr).ToSharedRef();
}

void SMainEditorWidgetBase::OnTabSpawned(const FName& TabIdentifier, const TSharedRef<SDockTab>& SpawnedTab)
{
	TWeakPtr<SDockTab>* const ExistingTab = SpawnedTabs.Find(TabIdentifier);
	if (!ExistingTab)
	{
		SpawnedTabs.Add(TabIdentifier, SpawnedTab);
	}
	else
	{
		check(!ExistingTab->IsValid());
		*ExistingTab = SpawnedTab;
	}
}

void SMainEditorWidgetBase::OnPersistLayout(const TSharedRef<FTabManager::FLayout>& LayoutToSave)
{
	FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, LayoutToSave);
}

FTabSpawnerEntry& SMainEditorWidgetBase::RegisterTabSpawner(const FName& TabId, const FOnSpawnTab& OnSpawnTab)
{
	return TabManager->RegisterTabSpawner(TabId, FOnSpawnTab::CreateLambda([this, OnSpawnTab](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
	{
		TSharedRef<SDockTab> SpawnedTab = OnSpawnTab.Execute(Args);
		OnTabSpawned(Args.GetTabId().TabType, SpawnedTab);
		return SpawnedTab;
	}));
}

void SMainEditorWidgetBase::UnRegisterTabSpawner(const FName& TabId)
{
	if(TabManager)
	{
		TabManager->UnregisterTabSpawner(TabId);
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
