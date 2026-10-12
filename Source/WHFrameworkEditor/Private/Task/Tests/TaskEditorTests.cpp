#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Task/TaskEditor.h"
#include "Task/Graph/TaskGraphOperations.h"
#include "Task/Graph/TaskAssetGraph.h"
#include "Task/Graph/TaskAssetGraphNode.h"
#include "Task/Graph/TaskAssetGraphSchema.h"
#include "Task/Slate/STaskGraphWidget.h"
#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskBase.h"
#include "Task/Base/TaskAssetReferenceTask.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "UObject/StrongObjectPtr.h"
#include "Framework/Commands/GenericCommands.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/ScopeExit.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"

namespace TaskGraphTests
{
	UTaskAsset* NewAsset()
	{
		UTaskAsset* Asset = NewObject<UTaskAsset>(GetTransientPackage(), NAME_None, RF_Transactional);
		Asset->InitializeGraph();
		return Asset;
	}

	UTaskBase* Add(UTaskAsset* Asset, FVector2D Position, UTaskBase* Parent = nullptr, bool bStart = false)
	{
		return FTaskGraphOperations::AddTask(Asset, UTaskBase::StaticClass(), Position, Parent, nullptr, bStart);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTaskGraphReachabilityTest, "WHFramework.Task.Graph.ReachabilityAndSiblingOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTaskGraphReachabilityTest::RunTest(const FString& Parameters)
{
	if(!TestNotNull(TEXT("需要编辑器事务环境"), GEditor))
	{
		return false;
	}
	TStrongObjectPtr<UTaskAsset> Asset(TaskGraphTests::NewAsset());
	UTaskBase* LowerRoot = TaskGraphTests::Add(Asset.Get(), FVector2D(0.f, 400.f));
	UTaskBase* UpperRoot = TaskGraphTests::Add(Asset.Get(), FVector2D(0.f, 100.f));
	UTaskBase* HighDescendant = TaskGraphTests::Add(Asset.Get(), FVector2D(320.f, -1000.f), LowerRoot);
	UTaskBase* LowerChild = TaskGraphTests::Add(Asset.Get(), FVector2D(320.f, 300.f), UpperRoot);
	UTaskBase* UpperChild = TaskGraphTests::Add(Asset.Get(), FVector2D(320.f, 200.f), UpperRoot);
	if(!LowerRoot || !UpperRoot || !HighDescendant || !LowerChild || !UpperChild)
	{
		AddError(TEXT("创建测试节点失败"));
		return false;
	}
	TestTrue(TEXT("断开树完整保存在作者集合"), Asset->AuthoredTasks.Num() == 5 && LowerRoot->GraphChildren.Contains(HighDescendant));
	TestTrue(TEXT("断开节点不进入编译结构"), Asset->RootTasks.IsEmpty() && Asset->TaskMap.IsEmpty() && LowerRoot->SubTasks.IsEmpty());
	TestTrue(TEXT("连接下方根到 Start"), FTaskGraphOperations::Connect(Asset.Get(), nullptr, LowerRoot, true));
	TestTrue(TEXT("连接上方根到 Start"), FTaskGraphOperations::Connect(Asset.Get(), nullptr, UpperRoot, true));
	TestTrue(TEXT("只排序 Start 的直接子节点"), Asset->RootTasks == TArray<UTaskBase*>({UpperRoot, LowerRoot}));
	TestTrue(TEXT("每个父节点独立排序"), UpperRoot->SubTasks == TArray<UTaskBase*>({UpperChild, LowerChild}));
	TestEqual(TEXT("Y 最小的孙节点不会成为全局根"), HighDescendant->ParentTask, LowerRoot);
	TestEqual(TEXT("父子层级保持"), HighDescendant->TaskHierarchy, 1);
	TestEqual(TEXT("同级索引从零独立计数"), UpperChild->TaskIndex, 0);
	UTaskBase* TieRight = TaskGraphTests::Add(Asset.Get(), FVector2D(640.f, 600.f), UpperRoot);
	UTaskBase* TieLeft = TaskGraphTests::Add(Asset.Get(), FVector2D(320.f, 600.f), UpperRoot);
	TestTrue(TEXT("同 Y 以 X 排序"), UpperRoot->SubTasks.IndexOfByKey(TieLeft) < UpperRoot->SubTasks.IndexOfByKey(TieRight));
	{
		const FScopedTransaction Transaction(NSLOCTEXT("TaskGraphTests", "Tie", "Move Task"));
		FTaskGraphOperations::SetNodePosition(Asset.Get(), TieRight, false, TieLeft->GraphPosition);
	}
	const bool bLeftFirst = TieLeft->TaskGUID < TieRight->TaskGUID;
	TestEqual(TEXT("同 Y/X 以稳定身份排序"), UpperRoot->SubTasks.IndexOfByKey(TieLeft) < UpperRoot->SubTasks.IndexOfByKey(TieRight), bLeftFirst);
	const TArray<UTaskBase*> RootsBeforeLayout = Asset->RootTasks;
	const TArray<UTaskBase*> ChildrenBeforeLayout = UpperRoot->SubTasks;
	FTaskGraphOperations::AutoLayout(Asset.Get());
	TestTrue(TEXT("布局保留按纵向位置确定的根顺序"), Asset->RootTasks == RootsBeforeLayout);
	TestTrue(TEXT("布局保留直接同级顺序，不恢复插入数组顺序"), UpperRoot->SubTasks == ChildrenBeforeLayout);
	for(int32 Left = 0; Left < Asset->AuthoredTasks.Num(); ++Left)
	{
		for(int32 Right = Left + 1; Right < Asset->AuthoredTasks.Num(); ++Right)
		{
			TestTrue(TEXT("业务节点各占独立行"), FMath::Abs(Asset->AuthoredTasks[Left]->GraphPosition.Y - Asset->AuthoredTasks[Right]->GraphPosition.Y) >= 200.f);
		}
	}
	FText Reason;
	TestFalse(TEXT("拒绝第二个父节点"), FTaskGraphOperations::CanConnect(Asset.Get(), LowerRoot, UpperChild, false, Reason));
	TestTrue(TEXT("断开 Start 不删除下方整棵作者树"), FTaskGraphOperations::DisconnectEdge(Asset.Get(), nullptr, LowerRoot, true));
	TestFalse(TEXT("断开后的孙节点不在 TaskMap"), Asset->TaskMap.Contains(HighDescendant->TaskGUID));
	TestTrue(TEXT("断开后保留内部连接"), LowerRoot->GraphChildren == TArray<UTaskBase*>({HighDescendant}));
	TestTrue(TEXT("断开后清理运行层级"), LowerRoot->SubTasks.IsEmpty() && HighDescendant->ParentTask == nullptr && HighDescendant->TaskHierarchy == INDEX_NONE);
	TestFalse(TEXT("拒绝断开子树内的循环"), FTaskGraphOperations::CanConnect(Asset.Get(), HighDescendant, LowerRoot, false, Reason));
	UTaskAssetReferenceTask* InvalidReference = CastChecked<UTaskAssetReferenceTask>(FTaskGraphOperations::AddTask(Asset.Get(), UTaskAssetReferenceTask::StaticClass(), FVector2D(0.f, 900.f)));
	TestTrue(TEXT("断开的未配置引用不会参与业务验证"), Asset->bCompiledValid);
	TestTrue(TEXT("连入未配置引用保留连线供修复"), FTaskGraphOperations::Connect(Asset.Get(), nullptr, InvalidReference, true));
	TestTrue(TEXT("失败立即清旧编译结果"), !Asset->bCompiledValid && Asset->RootTasks.IsEmpty() && Asset->TaskMap.IsEmpty() && !Asset->CompileErrors.IsEmpty());
	TestTrue(TEXT("修复断线恢复编译"), FTaskGraphOperations::DisconnectEdge(Asset.Get(), nullptr, InvalidReference, true) && Asset->bCompiledValid);
	TArray<FText> Errors;
	UTaskBase* Duplicate = TaskGraphTests::Add(Asset.Get(), FVector2D(0.f, 1200.f));
	const FString OriginalID = Duplicate->TaskGUID;
	const FGuid EquivalentID = FGuid::NewGuid();
	Duplicate->TaskGUID = EquivalentID.ToString(EGuidFormats::DigitsWithHyphens).ToUpper();
	InvalidReference->TaskGUID = EquivalentID.ToString(EGuidFormats::DigitsWithHyphensInBraces).ToLower();
	TestFalse(TEXT("同 GUID 不同字符串格式也拒绝"), Asset->CompileTasks(Errors));
	Duplicate->TaskGUID = OriginalID;
	InvalidReference->TaskGUID = FGuid::NewGuid().ToString();
	Errors.Reset();
	TestTrue(TEXT("修复身份后可重新编译"), Asset->CompileTasks(Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTaskGraphTransactionTest, "WHFramework.Task.Graph.TransactionsAndClipboard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTaskGraphTransactionTest::RunTest(const FString& Parameters)
{
	if(!TestNotNull(TEXT("需要编辑器事务环境"), GEditor))
	{
		return false;
	}
	TStrongObjectPtr<UTaskAsset> Asset(TaskGraphTests::NewAsset());
	TStrongObjectPtr<UTaskAsset> Destination(TaskGraphTests::NewAsset());
	UTaskBase* Root = TaskGraphTests::Add(Asset.Get(), FVector2D::ZeroVector, nullptr, true);
	UTaskBase* Child = TaskGraphTests::Add(Asset.Get(), FVector2D(320.f, 160.f), Root);
	if(!Root || !Child)
	{
		return false;
	}
	const FString ChildID = Child->TaskGUID;
	GEditor->UndoTransaction();
	TestTrue(TEXT("创建撤销恢复作者及编译集合"), Asset->AuthoredTasks.Num() == 1 && Asset->TaskMap.Num() == 1);
	GEditor->RedoTransaction();
	Child = Asset->FindAuthoredTask(ChildID);
	if(!TestNotNull(TEXT("重做按稳定身份重新取对象"), Child))
	{
		return false;
	}
	TestEqual(TEXT("重做恢复父关系"), Child->ParentTask, Root);
	Child->TaskDescription = NSLOCTEXT("TaskGraphTests", "PreservedText", "复制时保留文本身份");
	Child->bRequireExplicitTurnIn = true;
	FString Clipboard;
	TestTrue(TEXT("核心导出无需系统剪贴板"), FTaskGraphOperations::ExportTasks(Asset.Get(), {Root, Child}, Clipboard));
	const TArray<UTaskBase*> Copies = FTaskGraphOperations::ImportTasks(Destination.Get(), Clipboard, FVector2D(500.f, 500.f));
	if(!TestEqual(TEXT("粘贴选中节点数量"), Copies.Num(), 2))
	{
		return false;
	}
	UTaskBase* CopiedRoot = Copies[0];
	UTaskBase* CopiedChild = Copies[1];
	TestTrue(TEXT("粘贴根保持断开"), Destination->StartTasks.IsEmpty() && Destination->RootTasks.IsEmpty() && Destination->TaskMap.IsEmpty());
	TestTrue(TEXT("只保留选择内部连接"), CopiedRoot->GraphChildren == TArray<UTaskBase*>({CopiedChild}));
	TestTrue(TEXT("粘贴身份独立"), CopiedRoot->TaskGUID != Root->TaskGUID && CopiedChild->TaskGUID != ChildID);
	TestTrue(TEXT("文本及参数保留"), CopiedChild->TaskDescription.EqualTo(Child->TaskDescription) && CopiedChild->bRequireExplicitTurnIn);
	GEditor->UndoTransaction();
	TestTrue(TEXT("粘贴撤销移除所有作者节点"), Destination->AuthoredTasks.IsEmpty());
	GEditor->RedoTransaction();
	TestEqual(TEXT("粘贴重做恢复整个作者子图"), Destination->AuthoredTasks.Num(), 2);
	const int32 BeforeMalformed = Destination->AuthoredTasks.Num();
	TestTrue(TEXT("无效导入失败且不修改"), FTaskGraphOperations::ImportTasks(Destination.Get(), TEXT("{}"), FVector2D::ZeroVector).IsEmpty() && Destination->AuthoredTasks.Num() == BeforeMalformed);
	TestTrue(TEXT("删除仅删除选中父节点"), FTaskGraphOperations::DeleteTasks(Asset.Get(), {Root}));
	TestTrue(TEXT("未选择的子节点留下断开草稿"), Asset->AuthoredTasks.Contains(Child) && Asset->TaskMap.IsEmpty());
	GEditor->UndoTransaction();
	TestTrue(TEXT("删除撤销恢复模型连接与结构"), Root->GraphChildren.Contains(Child) && Child->ParentTask == Root && Asset->TaskMap.Num() == 2);
	TStrongObjectPtr<UBlueprint> Blueprint(FKismetEditorUtilities::CreateBlueprint(UTaskBase::StaticClass(), GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("TaskGraphTypeTest")), BPTYPE_Normal));
	UTaskBase* Replacement = FTaskGraphOperations::ChangeTaskType(Asset.Get(), Child, Blueprint->GeneratedClass);
	if(!TestNotNull(TEXT("完整蓝图类型替换"), Replacement))
	{
		return false;
	}
	TestTrue(TEXT("改类型保留身份和参数"), Replacement->TaskGUID == ChildID && Replacement->bRequireExplicitTurnIn);
	GEditor->UndoTransaction();
	TestEqual(TEXT("改类型撤销恢复原对象"), Asset->FindAuthoredTask(ChildID), Child);
	GEditor->RedoTransaction();
	TestEqual(TEXT("改类型重做按身份找到新对象"), Asset->FindAuthoredTask(ChildID), Replacement);
	GEditor->UndoTransaction();
	TStrongObjectPtr<UTaskAssetGraph> Graph(NewObject<UTaskAssetGraph>());
	Graph->TaskAsset = Asset.Get();
	Graph->Schema = UTaskAssetGraphSchema::StaticClass();
	Graph->OnTasksChanged.BindLambda([&Graph] { Graph->RebuildFromAsset(); });
	Graph->RebuildFromAsset();
	const FString StartID = Asset->StartGUID.ToString();
	UTaskAssetGraphNode* Start = Graph->FindNode(StartID);
	if(!TestNotNull(TEXT("图中唯一 Start"), Start))
	{
		return false;
	}
	TestTrue(TEXT("Start 不能复制或删除且没有输入"), !Start->CanDuplicateNode() && !Start->CanUserDeleteNode() && !Start->FindPin(TEXT("In")) && Start->FindPin(TEXT("Out")));
	const UTaskAssetGraphSchema* Schema = GetDefault<UTaskAssetGraphSchema>();
	Schema->BreakPinLinks(*Start->FindPin(TEXT("Out")), true);
	TestTrue(TEXT("Start 断开清空执行结构，作者节点和 Start 保留"), Asset->TaskMap.IsEmpty() && Graph->Nodes.Num() == 3);
	GEditor->UndoTransaction();
	Graph->RebuildFromAsset();
	Start = Graph->FindNode(StartID);
	TestTrue(TEXT("撤销后重新取节点和引脚"), Start && Start->FindPin(TEXT("Out")) && Start->FindPin(TEXT("Out"))->LinkedTo.Num() == 1 && Asset->TaskMap.Num() == 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTaskGraphMigrationTest, "WHFramework.Task.Graph.ExplicitMigration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTaskGraphMigrationTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UTaskAsset> Asset(NewObject<UTaskAsset>(GetTransientPackage(), NAME_None, RF_Transactional));
	UTaskBase* Root = NewObject<UTaskBase>(Asset.Get());
	UTaskBase* Child = NewObject<UTaskBase>(Asset.Get());
	Root->TaskGUID = TEXT("Commission.Root");
	Child->TaskGUID = TEXT("Commission.Objective");
	Root->SubTasks = {Child};
	Asset->RootTasks = {Root};
	TArray<FText> Errors;
	TestFalse(TEXT("旧定义不能自动进入运行编译"), Asset->CompileTasks(Errors));
	TestTrue(TEXT("保留旧树仅供显式迁移"), !Asset->bCompiledValid && Asset->RootTasks == TArray<UTaskBase*>({Root}) && Root->SubTasks == TArray<UTaskBase*>({Child}));
	Errors.Reset();
	TestTrue(TEXT("显式迁移保留所有节点"), FTaskGraphOperations::MigrateAsset(Asset.Get(), Errors) && Errors.IsEmpty());
	TestTrue(TEXT("稳定业务字符串身份保持"), Root->TaskGUID == TEXT("Commission.Root") && Child->TaskGUID == TEXT("Commission.Objective"));
	TestTrue(TEXT("迁移同时填充作者树和编译树"), Asset->AuthoredTasks.Num() == 2 && Root->GraphChildren.Contains(Child) && Child->ParentTask == Root);
	const FGuid StartID = Asset->StartGUID;
	FTaskGraphOperations::DisconnectEdge(Asset.Get(), nullptr, Root, true);
	TestFalse(TEXT("重复迁移不重连断开的节点"), FTaskGraphOperations::MigrateAsset(Asset.Get(), Errors));
	TestTrue(TEXT("保持原 Start 以及用户断线"), Asset->StartGUID == StartID && Asset->StartTasks.IsEmpty() && Asset->AuthoredTasks.Num() == 2);
	TStrongObjectPtr<UTaskAsset> Duplicated(DuplicateObject<UTaskAsset>(Asset.Get(), GetTransientPackage()));
	TestTrue(TEXT("持久化属性参与完整对象复制"), Duplicated->StartGUID == StartID && Duplicated->AuthoredTasks.Num() == 2 && Duplicated->StartTasks.IsEmpty());
	TestTrue(TEXT("子对象所有者重定向到副本"), Duplicated->AuthoredTasks[0]->GetTaskAsset() == Duplicated.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTaskGraphCommandsTest, "WHFramework.Task.Graph.EditorCommands", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTaskGraphCommandsTest::RunTest(const FString& Parameters)
{
	if(!TestNotNull(TEXT("需要编辑器"), GEditor))
	{
		return false;
	}
	TStrongObjectPtr<UTaskAsset> Asset(TaskGraphTests::NewAsset());
	UTaskBase* Root = TaskGraphTests::Add(Asset.Get(), FVector2D::ZeroVector, nullptr, true);
	if(!Root)
	{
		return false;
	}
	TSharedRef<FTaskEditor> Editor = FTaskEditorModule::Get().CreateTaskEditor(EToolkitMode::Standalone, nullptr, Asset.Get());
	ON_SCOPE_EXIT { GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Asset.Get()); };
	TSharedRef<STaskGraphWidget> View = Editor->GraphWidget.ToSharedRef();
	View->SelectTasks({Root});
	const TSharedPtr<FUICommandList> Commands = View->GetCommands();
	TestTrue(TEXT("重复命令走共同入口"), Commands->ExecuteAction(FGenericCommands::Get().Duplicate.ToSharedRef()));
	TestTrue(TEXT("重复节点保持断开"), Asset->AuthoredTasks.Num() == 2 && Asset->TaskMap.Num() == 1);
	Commands->ExecuteAction(FGenericCommands::Get().Undo.ToSharedRef());
	TestEqual(TEXT("快捷键撤销作者节点"), Asset->AuthoredTasks.Num(), 1);
	Commands->ExecuteAction(FGenericCommands::Get().Redo.ToSharedRef());
	TestEqual(TEXT("快捷键重做作者节点"), Asset->AuthoredTasks.Num(), 2);
	Commands->ExecuteAction(FGenericCommands::Get().Undo.ToSharedRef());
	FString PreviousClipboard;
	FPlatformApplicationMisc::ClipboardPaste(PreviousClipboard);
	const FString Probe = FGuid::NewGuid().ToString();
	FPlatformApplicationMisc::ClipboardCopy(*Probe);
	FString Readback;
	FPlatformApplicationMisc::ClipboardPaste(Readback);
	if(Readback != Probe)
	{
		AddInfo(TEXT("系统剪贴板不可用；核心 JSON 导入导出由独立测试覆盖，本测试停止剪切/粘贴步骤。"));
		return true;
	}
	ON_SCOPE_EXIT { FPlatformApplicationMisc::ClipboardCopy(*PreviousClipboard); };
	View->SelectTasks({Root});
	TestTrue(TEXT("复制命令可执行"), Commands->ExecuteAction(FGenericCommands::Get().Copy.ToSharedRef()));
	TestTrue(TEXT("粘贴命令可执行"), Commands->ExecuteAction(FGenericCommands::Get().Paste.ToSharedRef()));
	TestTrue(TEXT("粘贴不自动加入 Start"), Asset->AuthoredTasks.Num() == 2 && Asset->TaskMap.Num() == 1);
	Commands->ExecuteAction(FGenericCommands::Get().Undo.ToSharedRef());
	View->SelectTasks({Root});
	TestTrue(TEXT("剪切复制成功后才删除"), Commands->ExecuteAction(FGenericCommands::Get().Cut.ToSharedRef()));
	TestTrue(TEXT("剪切移除业务节点，Start 保留"), Asset->AuthoredTasks.IsEmpty() && Asset->StartGUID.IsValid());
	Commands->ExecuteAction(FGenericCommands::Get().Undo.ToSharedRef());
	TestEqual(TEXT("剪切撤销恢复原身份"), Asset->AuthoredTasks[0]->TaskGUID, Root->TaskGUID);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTaskGraphDragTest, "WHFramework.Task.Graph.SlateDraggingAndUndo", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTaskGraphDragTest::RunTest(const FString& Parameters)
{
	if(!TestNotNull(TEXT("需要编辑器"), GEditor) || !TestNull(TEXT("拖动测试不在 PIE 中修改资产"), GEditor->PlayWorld))
	{
		return false;
	}
	TStrongObjectPtr<UTaskAsset> Asset(TaskGraphTests::NewAsset());
	UTaskBase* Upper = TaskGraphTests::Add(Asset.Get(), FVector2D(0, 200), nullptr, true);
	UTaskBase* Lower = TaskGraphTests::Add(Asset.Get(), FVector2D(0, 400), nullptr, true);
	if(!TestNotNull(TEXT("上方任务"), Upper) || !TestNotNull(TEXT("下方任务"), Lower))
	{
		return false;
	}
	TSharedRef<FTaskEditor> Editor = FTaskEditorModule::Get().CreateTaskEditor(EToolkitMode::Standalone, nullptr, Asset.Get());
	ON_SCOPE_EXIT { GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Asset.Get()); };
	TSharedRef<STaskGraphWidget> View = Editor->GraphWidget.ToSharedRef();
	SGraphPanel* Panel = View->GetGraphEditor()->GetGraphPanel();
	if(!TestNotNull(TEXT("真实图面板"), Panel))
	{
		return false;
	}
	Panel->Update();
	UTaskAssetGraph* Graph = CastChecked<UTaskAssetGraph>(Panel->GetGraphObj());
	UTaskAssetGraphNode* Node = Graph->FindNode(Lower->TaskGUID);
	if(!TestNotNull(TEXT("按稳定身份找到拖动节点"), Node))
	{
		return false;
	}
	TSharedPtr<SGraphNode> Widget = Panel->GetNodeWidgetFromGuid(Node->NodeGuid);
	if(!TestTrue(TEXT("使用正式节点工厂生成的 Slate 控件"), Widget.IsValid()))
	{
		return false;
	}
	int32 GraphNotifications = 0;
	const FDelegateHandle Handler = Graph->AddOnGraphChangedHandler(FOnGraphChanged::FDelegate::CreateLambda(
		[&GraphNotifications](const FEdGraphEditAction&) { ++GraphNotifications; }));
	ON_SCOPE_EXIT { Graph->RemoveOnGraphChangedHandler(Handler); };
	{
		const FScopedTransaction Transaction(NSLOCTEXT("TaskGraphTests", "SlateDrag", "Drag Task"));
		SNodePanel::SNode::FNodeSet NodeFilter;
		Widget->MoveTo(FVector2f(80.6f, 100.6f), NodeFilter);
		TestEqual(TEXT("作者位置与图节点统一取整"), Lower->GraphPosition, FVector2D(81, 101));
		Widget->MoveTo(FVector2f(120, 120), NodeFilter);
		TestEqual(TEXT("同一移动集合不会重复移动"), Lower->GraphPosition, FVector2D(81, 101));
		NodeFilter.Reset();
		Widget->MoveTo(FVector2f(100, 90), NodeFilter);
	}
	TestEqual(TEXT("拖动中不刷新或替换正在使用的图控件"), GraphNotifications, 0);
	TestTrue(TEXT("连续移动保留原 Slate 控件"), Panel->GetNodeWidgetFromGuid(Node->NodeGuid) == Widget);
	TestEqual(TEXT("图位置同步更新"), Node->GetPosition(), FVector2f(100, 90));
	TestTrue(TEXT("拖动同步直接同级纵向顺序"), Asset->RootTasks == TArray<UTaskBase*>({Lower, Upper}));
	TestTrue(TEXT("标题同步排序序号"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().StartsWith(TEXT("[1]")));
	GEditor->UndoTransaction();
	TestEqual(TEXT("一次撤销恢复整个连续拖动"), Lower->GraphPosition, FVector2D(0, 400));
	TestTrue(TEXT("撤销同时恢复编译顺序"), Asset->RootTasks == TArray<UTaskBase*>({Upper, Lower}));
	GEditor->RedoTransaction();
	TestEqual(TEXT("重做恢复最终拖动位置"), Lower->GraphPosition, FVector2D(100, 90));
	TestTrue(TEXT("重做恢复编译顺序"), Asset->RootTasks == TArray<UTaskBase*>({Lower, Upper}));
	View->Rebuild();
	Node = Graph->FindNode(Lower->TaskGUID);
	TestTrue(TEXT("重建后稳定身份及最终位置仍有效"), Node && Node->GetPosition() == FVector2f(100, 90));
	return true;
}

#endif
