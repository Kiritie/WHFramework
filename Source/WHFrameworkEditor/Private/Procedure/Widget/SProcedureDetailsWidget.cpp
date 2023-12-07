// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Widget/SProcedureDetailsWidget.h"

#include "SlateOptMacros.h"
#include "Procedure/ProcedureEditor.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Widget/SProcedureListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SProcedureDetailsWidget::SProcedureDetailsWidget()
{
	WidgetName = FName("ProcedureDetailsWidget");
	WidgetType = EEditorWidgetType::Child;
}

void SProcedureDetailsWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
	
	ProcedureEditor = InArgs._ProcedureEditor;

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

	if(ProcedureEditor.Pin()->ListWidget->bDefaults)
	{
		DetailsView->SetObject(ProcedureEditor.Pin()->GetEditingAsset());
	}
	else if(ProcedureEditor.Pin()->ListWidget->SelectedProcedureListItems.Num() > 0)
	{
		TArray<UObject*> Procedures = TArray<UObject*>();
		for(auto Iter : ProcedureEditor.Pin()->ListWidget->SelectedProcedureListItems)
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
		DetailsView->SetObject(nullptr);
	}
}

void SProcedureDetailsWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
