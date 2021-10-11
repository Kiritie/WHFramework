// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/SProcedureMainWidget.h"

#include "MainModule.h"
#include "SlateOptMacros.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModuleBPLibrary.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleBPLibrary.h"
#include "Widget/SProcedureDetailWidget.h"
#include "Widget/SProcedureListWidget.h"
#include "Widget/SProcedureStatusWidget.h"
#include "Widget/SProcedureToolbarWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProcedureMainWidget::Construct(const FArguments& InArgs)
{
	// for (const FWorldContext& Context : GEngine->GetWorldContexts())
	// {
	// 	if(MainModule && ProcedureModule) break;
	// }
	MainModule = UMainModuleBPLibrary::GetMainModule();
	ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>();

	if(!MainModule || !ProcedureModule) return;
	
	GConfig->GetBool(TEXT("/Script/WHProcedureEditor.WHProcedureEditorSettings"), TEXT("bShowListPanel"), bShowListPanel, GProcedureEditorIni);
	GConfig->GetBool(TEXT("/Script/WHProcedureEditor.WHProcedureEditorSettings"), TEXT("bShowDetailPanel"), bShowDetailPanel, GProcedureEditorIni);
	GConfig->GetBool(TEXT("/Script/WHProcedureEditor.WHProcedureEditorSettings"), TEXT("bShowStatusPanel"), bShowStatusPanel, GProcedureEditorIni);

	SAssignNew(ListWidget, SProcedureListWidget)
	.MainModule(MainModule)
	.ProcedureModule(ProcedureModule)
	.Visibility_Lambda([this](){ return bShowListPanel ? EVisibility::Visible : EVisibility::Collapsed;});

	SAssignNew(DetailWidget, SProcedureDetailWidget)
	.ProcedureModule(ProcedureModule)
	.ListWidget(ListWidget)
	.Visibility_Lambda([this](){ return bShowDetailPanel ? EVisibility::Visible : EVisibility::Collapsed;});

	SAssignNew(StatusWidget, SProcedureStatusWidget)
	.ListWidget(ListWidget)
	.Visibility_Lambda([this](){ return bShowStatusPanel ? EVisibility::Visible : EVisibility::Collapsed;});

	SAssignNew(ToolbarWidget, SProcedureToolbarWidget)
	.MainWidget(SharedThis(this))
	.ListWidget(ListWidget);

	ChildSlot
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
			.HAlign(HAlign_Fill)
			.FillWidth(1)
			.Padding(1)
			[
				ToolbarWidget.ToSharedRef()
			]
		]

		+ SVerticalBox::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		.FillHeight(1)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.Padding(1)
			.AutoWidth()
			[
				ListWidget.ToSharedRef()
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.FillWidth(1)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.Padding(1)
			.AutoWidth()
			[
				DetailWidget.ToSharedRef()
			]
		]

		+ SVerticalBox::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.Padding(1)
			.FillWidth(1)
			[
				StatusWidget.ToSharedRef()
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION
