#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Dialogue/DialogueEditor.h"
#include "Dialogue/DialogueGraphClipboard.h"
#include "Dialogue/Slate/SDialogueViewportWidget.h"
#include "Dialogue/Base/DialogueAsset.h"
#include "Dialogue/Conditions/DialogueCondition_And.h"
#include "Dialogue/Conditions/DialogueCondition_Task.h"
#include "Dialogue/Events/DialogueEvent_Task.h"
#include "UObject/StrongObjectPtr.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/CommandLine.h"
#include "UObject/UObjectHash.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueEditorTest, "WHFramework.Dialogue.Editor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDialogueEditorTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UDialogueAsset> Source(NewObject<UDialogueAsset>(GetTransientPackage(), NAME_None, RF_Transactional));
	TStrongObjectPtr<UDialogueAsset> Destination(NewObject<UDialogueAsset>(GetTransientPackage(), NAME_None, RF_Transactional));
	FDialogueNode Start; Start.id = 0; Start.Links = {1}; Start.Coordinates = FVector2D(130, 150);
	FDialogueNode First; First.id = 1; First.Links = {2, 3}; First.Coordinates = FVector2D(420, 150);
	First.Text = NSLOCTEXT("DialogueEditorTest", "Line", "A preserved dialogue line");
	FDialogueNode Second; Second.id = 2; Second.isPlayer = true; Second.Links = {1}; Second.Coordinates = FVector2D(750, 110);
	Second.Text = NSLOCTEXT("DialogueEditorTest", "Reply", "Accept the quest");
	UDialogueEvent_Task* Event = NewObject<UDialogueEvent_Task>(Source.Get());
	Event->Action = EDialogueTaskAction::TurnIn;
	Event->Task.TaskGUID = FGuid::NewGuid().ToString();
	First.Events.Add(Event);
	UDialogueCondition_And* All = NewObject<UDialogueCondition_And>(Source.Get());
	UDialogueCondition_Task* Condition = NewObject<UDialogueCondition_Task>(All);
	Condition->RequiredStage = ETaskStage::ReadyToTurnIn;
	All->AndConditions.Add(Condition);
	First.Conditions.Add(All);
	FDialogueNode External; External.id = 3; External.isPlayer = true; External.Coordinates = FVector2D(750, 330);
	External.Text = NSLOCTEXT("DialogueEditorTest", "External", "Ask about the quest");
	Source->Data = {Start, First, Second, External};
	Source->NextNodeId = 4;
	Destination->Data = {Start};
	Destination->Data[0].Links.Reset();
	Destination->NextNodeId = 2;

	FString Clipboard;
	TestTrue(TEXT("复制节点与内嵌对象"), FDialogueGraphClipboard::ExportNodes(Source.Get(), {1, 2}, Clipboard));
	TArray<int32> IDs;
	TestTrue(TEXT("跨资产粘贴"), FDialogueGraphClipboard::ImportNodes(Destination.Get(), Clipboard, FVector2D(300, 100), IDs));
	if (!TestEqual(TEXT("粘贴节点数量"), IDs.Num(), 2)) return false;
	const FDialogueNode Copied = Destination->GetNodeById(IDs[0]);
	const FDialogueNode Reply = Destination->GetNodeById(IDs[1]);
	FString OriginalText, CopiedText;
	FTextStringHelper::WriteToBuffer(OriginalText, First.Text);
	FTextStringHelper::WriteToBuffer(CopiedText, Copied.Text);
	TestEqual(TEXT("文本及本地化命名空间和键保持"), CopiedText, OriginalText);
	TestTrue(TEXT("链接重映射保序且移除外部链接"), Copied.Links == TArray<int32>{IDs[1]});
	TestTrue(TEXT("回连不与新旧 ID 碰撞"), Reply.Links == TArray<int32>{IDs[0]});
	if (TestEqual(TEXT("事件保留"), Copied.Events.Num(), 1))
	{
		const UDialogueEvent_Task* CopiedEvent = Cast<UDialogueEvent_Task>(Copied.Events[0]);
		if (TestNotNull(TEXT("事件类型保留"), CopiedEvent))
		{
			TestTrue(TEXT("事件实例独立"), CopiedEvent != Event);
			TestEqual(TEXT("事件归属目标资产"), CopiedEvent->GetTypedOuter<UDialogueAsset>(), Destination.Get());
			TestEqual(TEXT("事件动作参数保留"), CopiedEvent->Action, Event->Action);
			TestEqual(TEXT("任务身份参数保留"), CopiedEvent->Task.TaskGUID, Event->Task.TaskGUID);
		}
	}
	if (TestEqual(TEXT("条件保留"), Copied.Conditions.Num(), 1))
	{
		const UDialogueCondition_And* CopiedAll = Cast<UDialogueCondition_And>(Copied.Conditions[0]);
		if (TestNotNull(TEXT("组合条件类型保留"), CopiedAll) && TestEqual(TEXT("嵌套条件保留"), CopiedAll->AndConditions.Num(), 1))
		{
			const UDialogueCondition_Task* CopiedCondition = Cast<UDialogueCondition_Task>(CopiedAll->AndConditions[0]);
			if (TestNotNull(TEXT("嵌套条件类型保留"), CopiedCondition))
			{
				TestTrue(TEXT("嵌套条件实例独立"), CopiedCondition != Condition);
				TestEqual(TEXT("嵌套条件参数保留"), CopiedCondition->RequiredStage, Condition->RequiredStage);
				TestEqual(TEXT("嵌套条件归属目标资产"), CopiedCondition->GetTypedOuter<UDialogueAsset>(), Destination.Get());
			}
		}
	}
	GEditor->UndoTransaction();
	TestEqual(TEXT("粘贴撤销"), Destination->Data.Num(), 1);
	GEditor->RedoTransaction();
	TestEqual(TEXT("粘贴重做"), Destination->Data.Num(), 3);
	TestFalse(TEXT("忽略无关剪贴板"), FDialogueGraphClipboard::ImportNodes(Destination.Get(), TEXT("{}"), FVector2D::ZeroVector, IDs));
	TSharedRef<FDialogueEditor> FirstEditor = FDialogueEditorModule::Get().CreateDialogueEditor(EToolkitMode::Standalone, nullptr, Source.Get());
	TSharedRef<FDialogueEditor> SecondEditor = FDialogueEditorModule::Get().CreateDialogueEditor(EToolkitMode::Standalone, nullptr, Destination.Get());
	if (TestTrue(TEXT("框架对话图窗口创建"), FirstEditor->DialogueViewportWidget.IsValid() && SecondEditor->DialogueViewportWidget.IsValid()))
	{
		FirstEditor->DialogueViewportWidget->SelectNodes(1);
		SecondEditor->DialogueViewportWidget->SelectNodes(2);
		TestEqual(TEXT("第一窗口选择独立"), FirstEditor->GetEditingState()->CurrentNodeId, 1);
		TestEqual(TEXT("第二窗口选择独立"), SecondEditor->GetEditingState()->CurrentNodeId, 2);
		FString Preview;
		if (FParse::Value(FCommandLine::Get(), TEXT("DialoguePreview="), Preview))
		{
			FWidgetRenderer Renderer(true);
			UTextureRenderTarget2D* Target = Renderer.DrawWidget(FirstEditor->DialogueViewportWidget.ToSharedRef(), FVector2D(1000, 480));
			FImage Image;
			if (TestTrue(TEXT("对话图渲染"), FImageUtils::GetRenderTargetImage(Target, Image)))
			{
				if (Image.Format == ERawImageFormat::BGRA8) Image.GammaSpace = EGammaSpace::sRGB;
				TestTrue(TEXT("对话图预览导出"), FImageUtils::SaveImageByExtension(*Preview, Image));
			}
		}
	}
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Source.Get());
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Destination.Get());
	return true;
}

#endif
