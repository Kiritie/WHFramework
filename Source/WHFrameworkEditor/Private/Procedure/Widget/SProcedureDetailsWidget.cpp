// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Widget/SProcedureDetailsWidget.h"

#include "SlateOptMacros.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Widget/SProcedureEditorWidget.h"
#include "Procedure/Widget/SProcedureListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SProcedureDetailsWidget::SProcedureDetailsWidget()
{
	WidgetName = FName("ProcedureDetailsWidget");
	WidgetType = EEditorWidgetType::Child;
	ProcedureModule = nullptr;
}

void SProcedureDetailsWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	ProcedureModule = InArgs._ProcedureModule;
	ListWidget = InArgs._ListWidget;

	if(!ProcedureModule || !ListWidget) return;

	ListWidget->OnSelectProcedureListItemsDelegate.BindRaw(this, &SProcedureDetailsWidget::OnSelectProcedureListItem);

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

void SProcedureDetailsWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void SProcedureDetailsWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SProcedureDetailsWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();
}

void SProcedureDetailsWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

void SProcedureDetailsWidget::UpdateDetailsView()
{
	if(SelectedProcedureListItems.Num() > 0)
	{
		TArray<UObject*> Procedures = TArray<UObject*>();
		for(auto Iter : SelectedProcedureListItems)
		{
			if(Iter->Procedure)
			{
				Procedures.Add(Iter->Procedure);
			}
		}
		DetailsView->SetObjects(Procedures);
	}
	else
	{
		DetailsView->SetObject(ProcedureModule);
	}
}

void SProcedureDetailsWidget::OnSelectProcedureListItem(TArray<TSharedPtr<FProcedureListItem>> ProcedureListItem)
{
	SelectedProcedureListItems = ProcedureListItem;

	UpdateDetailsView();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
