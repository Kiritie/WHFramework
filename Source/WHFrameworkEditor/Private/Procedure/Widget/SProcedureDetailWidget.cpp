// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Widget/SProcedureDetailWidget.h"

#include "ISinglePropertyView.h"
#include "SlateOptMacros.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Widget/SProcedureListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SProcedureDetailWidget::Construct(const FArguments& InArgs)
{
	ListWidget = InArgs._ListWidget;
	ProcedureModule = InArgs._ProcedureModule;

	if(!ProcedureModule || !ProcedureModule->IsValidLowLevel() || !ListWidget) return;

	ListWidget->OnSelectProcedureListItemsDelegate.BindRaw(this, &SProcedureDetailWidget::OnSelectProcedureListItem);

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
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Center)
					.AutoWidth()
					.Padding(5.f, 2.f, 5.f, 2.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Detail Panel")))
					]
				]

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

void SProcedureDetailWidget::UpdateDetailsView()
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

void SProcedureDetailWidget::OnSelectProcedureListItem(TArray<TSharedPtr<FProcedureListItem>> ProcedureListItem)
{
	SelectedProcedureListItems = ProcedureListItem;

	UpdateDetailsView();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
