// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Widget/SStepDetailsWidget.h"

#include "ISinglePropertyView.h"
#include "SlateOptMacros.h"
#include "Step/StepModule.h"
#include "Step/Base/StepBase.h"
#include "Step/Widget/SStepListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SStepDetailsWidget::SStepDetailsWidget()
{
	WidgetName = FName("StepDetailsWidget");
	WidgetType = EEditorWidgetType::Child;
	StepModule = nullptr;
}

void SStepDetailsWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	ListWidget = InArgs._ListWidget;
	StepModule = InArgs._StepModule;

	if(!StepModule || !StepModule->IsValidLowLevel() || !ListWidget) return;

	ListWidget->OnSelectStepListItemsDelegate.BindRaw(this, &SStepDetailsWidget::OnSelectStepListItem);

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

void SStepDetailsWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void SStepDetailsWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SStepDetailsWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();
}

void SStepDetailsWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

void SStepDetailsWidget::UpdateDetailsView()
{
	if(SelectedStepListItems.Num() > 0)
	{
		TArray<UObject*> Steps = TArray<UObject*>();
		for(auto Iter : SelectedStepListItems)
		{
			if(Iter->Step)
			{
				Steps.Add(Iter->Step);
			}
		}
		DetailsView->SetObjects(Steps);
	}
	else
	{
		DetailsView->SetObject(StepModule);
	}
}

void SStepDetailsWidget::OnSelectStepListItem(TArray<TSharedPtr<FStepListItem>> StepListItem)
{
	SelectedStepListItems = StepListItem;

	UpdateDetailsView();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
