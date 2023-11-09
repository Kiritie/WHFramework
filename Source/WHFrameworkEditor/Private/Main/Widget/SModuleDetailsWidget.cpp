// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Widget/SModuleDetailsWidget.h"

#include "SlateOptMacros.h"
#include "Main/MainModule.h"
#include "Main/Base/ModuleBase.h"
#include "Main/Widget/SModuleEditorWidget.h"
#include "Main/Widget/SModuleListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SModuleDetailsWidget::SModuleDetailsWidget()
{
	WidgetName = FName("ModuleDetailsWidget");
	WidgetType = EEditorWidgetType::Child;
	MainModule = nullptr;
}

void SModuleDetailsWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	MainModule = InArgs._MainModule;
	ListWidget = InArgs._ListWidget;

	if(!MainModule || !ListWidget) return;

	ListWidget->OnSelectModuleListItemsDelegate.BindRaw(this, &SModuleDetailsWidget::OnSelectModuleListItem);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ObjectsUseNameArea;
	DetailsViewArgs.bHideSelectionTip = false;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	ChildSlot
	[
		SNew(SBorder)
		.Padding(1.f)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SBox)
			.WidthOverride(420)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.FillHeight(1)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						DetailsView.ToSharedRef()
					]
				]
			]
		]
	];

	UpdateDetailsView();
}

void SModuleDetailsWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void SModuleDetailsWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SModuleDetailsWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();
}

void SModuleDetailsWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

void SModuleDetailsWidget::UpdateDetailsView()
{
	if(SelectedModuleListItems.Num() > 0)
	{
		TArray<UObject*> Modules = TArray<UObject*>();
		for(auto Iter : SelectedModuleListItems)
		{
			if(Iter->Module)
			{
				Modules.Add(Iter->Module);
			}
		}
		DetailsView->SetObjects(Modules);
	}
	else
	{
		DetailsView->SetObject(MainModule);
	}
}

void SModuleDetailsWidget::OnSelectModuleListItem(TArray<TSharedPtr<FModuleListItem>> ModuleListItem)
{
	SelectedModuleListItems = ModuleListItem;

	UpdateDetailsView();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
