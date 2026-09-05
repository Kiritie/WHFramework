#include "Task/Slate/STaskStatusWidget.h"

#include "Task/TaskEditor.h"
#include "Task/Slate/STaskGraphWidget.h"
#include "Task/Base/TaskAsset.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "TaskGraphStatus"

const FName STaskStatusWidget::WidgetName = FName("TaskStatusWidget");

STaskStatusWidget::STaskStatusWidget() { WidgetType = EEditorWidgetType::Child; }

void STaskStatusWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
	TaskEditor = InArgs._TaskEditor;
	ChildSlot[
		SNew(SBorder).Padding(FMargin(10, 4)).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))[
			SNew(STextBlock).Text_Lambda([this]
			{
				const TSharedPtr<FTaskEditor> Editor = TaskEditor.Pin();
				if (!Editor || !Editor->GraphWidget) return FText::GetEmpty();
				const UTaskAsset* Asset = Editor->GetEditingAsset<UTaskAsset>();
				const bool bEditable = Editor->GraphWidget->CanEdit();
				return FText::Format(LOCTEXT("Status", "Task Graph  |  Tasks: {0}  |  Selected: {1}  |  Roots: {2}  |  {3}"),
					FText::AsNumber(Asset->TaskMap.Num()), FText::AsNumber(Editor->GraphWidget->GetSelectedTasks().Num()),
					FText::AsNumber(Asset->RootTasks.Num()), bEditable ?
					LOCTEXT("EditHint", "Right-click to edit; Out > In to connect; Ctrl+Z / Ctrl+Y to undo / redo") :
					LOCTEXT("ReadOnly", "Read only"));
			})
		]
	];
}
void STaskStatusWidget::OnCreate() { SEditorWidgetBase::OnCreate(); }
void STaskStatusWidget::OnReset() { SEditorWidgetBase::OnReset(); }
void STaskStatusWidget::OnRefresh() { SEditorWidgetBase::OnRefresh(); }
void STaskStatusWidget::OnDestroy() { SEditorWidgetBase::OnDestroy(); }

#undef LOCTEXT_NAMESPACE
