#include "Step/Slate/SStepStatusWidget.h"

#include "Step/StepEditor.h"
#include "Step/Slate/SStepGraphWidget.h"
#include "Step/Graph/StepAssetGraph.h"
#include "Step/Base/StepAsset.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"

const FName SStepStatusWidget::WidgetName = FName("StepStatusWidget");

SStepStatusWidget::SStepStatusWidget()
{
	WidgetType = EEditorWidgetType::Child;
}

void SStepStatusWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
	StepEditor = InArgs._StepEditor;
	ChildSlot
	[
		SNew(SBorder).Padding(FMargin(10, 4)).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(STextBlock).Text_Lambda([this]
			{
				const TSharedPtr<FStepEditor> Editor = StepEditor.Pin();
				if(!Editor || !Editor->GraphWidget)
				{
					return FText::GetEmpty();
				}
				const UStepAsset* Asset = Editor->GetEditingAsset<UStepAsset>();
				const UStepAssetGraph* Graph = Editor->GraphWidget->GetGraph();
				if(!Graph->Errors.IsEmpty())
				{
					return FText::Format(NSLOCTEXT("StepGraph", "Errors", "Compile errors: {0} | {1}"), FText::AsNumber(Graph->Errors.Num()), Graph->Errors[0]);
				}
				return FText::Format(NSLOCTEXT("StepGraph", "Status", "Steps: {0} | Reachable: {1} | Detached: {2} | Roots: {3} | Selected: {4}"),
					FText::AsNumber(Asset->Steps.Num()), FText::AsNumber(Asset->CompiledGraph.Nodes.Num()),
					FText::AsNumber(Asset->Steps.Num() - Asset->CompiledGraph.Nodes.Num()),
					FText::AsNumber(Asset->CompiledGraph.RootGUIDs.Num()), FText::AsNumber(Editor->GraphWidget->GetSelectedSteps().Num()));
			})
		]
	];
}

void SStepStatusWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void SStepStatusWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SStepStatusWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();
}

void SStepStatusWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}
