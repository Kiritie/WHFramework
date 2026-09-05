#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Task/TaskEditor.h"
#include "Task/Graph/TaskGraphOperations.h"
#include "Task/Graph/TaskAssetGraph.h"
#include "Task/Graph/TaskAssetGraphNode.h"
#include "Task/Graph/TaskAssetGraphSchema.h"
#include "Task/Slate/STaskGraphWidget.h"
#include "Task/Slate/STaskDetailsWidget.h"
#include "Task/Slate/STaskStatusWidget.h"
#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskBase.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "UObject/StrongObjectPtr.h"
#include "IDetailsView.h"
#include "SGraphPanel.h"
#include "SGraphNode.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTaskEditorTest, "WHFramework.Task.Editor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTaskEditorTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UTaskAsset> Asset(NewObject<UTaskAsset>(GetTransientPackage(), NAME_None, RF_Transactional));
	TStrongObjectPtr<UTaskAsset> Destination(NewObject<UTaskAsset>(GetTransientPackage(), NAME_None, RF_Transactional));
	UTaskBase* Root = FTaskGraphOperations::AddTask(Asset.Get(), UTaskBase::StaticClass(), FVector2D(60, 80));
	if (!TestNotNull(TEXT("创建根任务"), Root)) return false;
	const FString RootID = Root->TaskGUID;
	GEditor->UndoTransaction();
	TestTrue(TEXT("创建撤销"), Asset->RootTasks.IsEmpty());
	GEditor->RedoTransaction();
	TestEqual(TEXT("创建重做保留身份"), Asset->RootTasks[0]->TaskGUID, RootID);
	Asset->RebuildTaskMap(false);
	UTaskBase* Child = FTaskGraphOperations::AddTask(Asset.Get(), UTaskBase::StaticClass(), FVector2D(400, 80), Root);
	UTaskBase* Leaf = FTaskGraphOperations::AddTask(Asset.Get(), UTaskBase::StaticClass(), FVector2D(720, 80), Child);
	UTaskBase* Other = FTaskGraphOperations::AddTask(Asset.Get(), UTaskBase::StaticClass(), FVector2D(400, 350), Root);
	Root->TaskDisplayName = NSLOCTEXT("TaskEditorTest", "Root", "寻找失踪的调查员");
	Child->TaskDisplayName = NSLOCTEXT("TaskEditorTest", "Child", "调查营地");
	Leaf->TaskDisplayName = NSLOCTEXT("TaskEditorTest", "Leaf", "检查遗留的线索");
	Other->TaskDisplayName = NSLOCTEXT("TaskEditorTest", "Other", "向守卫报告");
	Child->TaskDescription = NSLOCTEXT("TaskEditorTest", "Description", "检查营地中的物品，寻找调查员的去向。");
	Child->bRequireExplicitTurnIn = true;
	Child->AutoExecuteTaskTime = 4.5f;
	FTaskReference Requirement; Requirement.Asset = Asset.Get(); Requirement.TaskGUID = Child->TaskGUID;
	Other->Prerequisites = {Requirement};
	Other->ClearFlags(RF_Transactional);
	TSharedRef<FTaskEditor> Editor = FTaskEditorModule::Get().CreateTaskEditor(EToolkitMode::Standalone, nullptr, Asset.Get());
	ON_SCOPE_EXIT { GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Asset.Get()); };
	TestTrue(TEXT("已有节点打开编辑器后支持事务"), Other->HasAnyFlags(RF_Transactional));
	const TSharedRef<STaskGraphWidget> View = Editor->GraphWidget.ToSharedRef();
	const TSharedPtr<FUICommandList> Commands = View->GetCommands();
	auto Undo = [&] { TestTrue(TEXT("撤销命令执行"), Commands->ExecuteAction(FGenericCommands::Get().Undo.ToSharedRef())); };
	auto Redo = [&] { TestTrue(TEXT("重做命令执行"), Commands->ExecuteAction(FGenericCommands::Get().Redo.ToSharedRef())); };
	auto Graph = [&] { return CastChecked<UTaskAssetGraph>(View->GetGraphEditor()->GetGraphPanel()->GetGraphObj()); };
	auto Node = [&](UTaskBase* Task) -> UTaskAssetGraphNode*
	{
		for (UEdGraphNode* Item : Graph()->Nodes)
			if (UTaskAssetGraphNode* Found = Cast<UTaskAssetGraphNode>(Item); Found && Found->Task == Task) return Found;
		return nullptr;
	};
	const UTaskAssetGraphSchema* Schema = GetDefault<UTaskAssetGraphSchema>();
	View->SelectTasks({Child});
	TestEqual(TEXT("详情跟随当前图节点"), Editor->DetailsWidget->DetailsView->GetSelectedObjects()[0].Get(), static_cast<UObject*>(Child));
	TestNotNull(TEXT("输入端口 In"), Node(Child)->FindPin(TEXT("In")));
	TestNotNull(TEXT("输出端口 Out"), Node(Child)->FindPin(TEXT("Out")));
	TestNull(TEXT("旧端口 Parent 已移除"), Node(Child)->FindPin(TEXT("Parent")));

	UTaskBase* Inserted = FTaskGraphOperations::AddTask(Asset.Get(), UTaskBase::StaticClass(), FVector2D(560, 180), nullptr, Leaf);
	View->Rebuild();
	TestEqual(TEXT("从 In 创建节点保留原上游"), Inserted->ParentTask, Child);
	TestEqual(TEXT("从 In 创建节点连接原节点"), Leaf->ParentTask, Inserted);
	Undo();
	TestEqual(TEXT("创建中间节点撤销"), Leaf->ParentTask, Child);
	Redo();
	TestEqual(TEXT("创建中间节点重做"), Leaf->ParentTask, Inserted);
	Undo();

	TStrongObjectPtr<UBlueprint> Blueprint(FKismetEditorUtilities::CreateBlueprint(UTaskBase::StaticClass(), GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("TaskEditorTest")), BPTYPE_Normal));
	UTaskBase* Changed = FTaskGraphOperations::ChangeTaskType(Asset.Get(), Child, Blueprint->GeneratedClass);
	if (!TestNotNull(TEXT("切换为蓝图类型"), Changed)) return false;
	View->Rebuild();
	TestEqual(TEXT("类型切换保留 GUID"), Changed->TaskGUID, Child->TaskGUID);
	TestTrue(TEXT("类型切换保留参数"), Changed->bRequireExplicitTurnIn && Changed->AutoExecuteTaskTime == 4.5f);
	TestEqual(TEXT("类型切换保留原子节点实例"), Changed->SubTasks[0], Leaf);
	TestEqual(TEXT("类型切换修正父指针"), Leaf->ParentTask, Changed);
	TestTrue(TEXT("类型切换恢复选择"), View->GetSelectedTasks() == TArray<UTaskBase*>{Changed});
	Undo();
	TestEqual(TEXT("类型切换撤销"), Asset->TaskMap.FindRef(Child->TaskGUID), Child);
	TestTrue(TEXT("类型切换撤销恢复选择"), View->GetSelectedTasks() == TArray<UTaskBase*>{Child});
	Redo();
	TestEqual(TEXT("类型切换重做"), Asset->TaskMap.FindRef(Child->TaskGUID), Changed);
	Undo();

	UTaskBase* BlueprintTask = FTaskGraphOperations::AddTask(Destination.Get(), Blueprint->GeneratedClass, FVector2D(120, 160));
	BlueprintTask->TaskDescription = NSLOCTEXT("TaskEditorTest", "BlueprintText", "蓝图任务的复制参数");
	FString BlueprintClipboard;
	TestTrue(TEXT("复制派生蓝图任务"), FTaskGraphOperations::ExportTasks(Destination.Get(), {BlueprintTask}, BlueprintClipboard));
	const TArray<UTaskBase*> BlueprintCopies = FTaskGraphOperations::ImportTasks(Destination.Get(), BlueprintClipboard, FVector2D(340, 260));
	if (TestEqual(TEXT("粘贴派生蓝图任务"), BlueprintCopies.Num(), 1))
	{
		TestEqual(TEXT("剪贴板保留蓝图类"), BlueprintCopies[0]->GetClass(), Blueprint->GeneratedClass.Get());
		TestTrue(TEXT("剪贴板保留蓝图实例文本"), BlueprintCopies[0]->TaskDescription.EqualTo(BlueprintTask->TaskDescription));
	}
	Undo();
	Undo();

	const FText Description = Child->TaskDescription;
	{
		const FScopedTransaction Transaction(NSLOCTEXT("TaskEditorTest", "Properties", "Edit Task Properties"));
		Child->Modify();
		Child->TaskDescription = FText::FromString(TEXT("已修改的描述"));
		Child->Objectives.AddDefaulted();
	}
	Editor->OnTaskPropertyChanged(FPropertyChangedEvent(FindFProperty<FProperty>(UTaskBase::StaticClass(), GET_MEMBER_NAME_CHECKED(UTaskBase, Objectives))));
	TestEqual(TEXT("未完成的目标配置仍显示整张图"), Graph()->Nodes.Num(), 4);
	TArray<FText> Errors;
	TestFalse(TEXT("运行定义仍严格校验未配置目标"), Asset->ValidateTasks(Errors));
	Undo();
	TestTrue(TEXT("详情属性撤销"), Child->TaskDescription.EqualTo(Description) && Child->Objectives.IsEmpty());
	Redo();
	TestEqual(TEXT("详情属性重做"), Child->Objectives.Num(), 1);
	Undo();

	FString Clipboard;
	TestTrue(TEXT("复制选择子树"), FTaskGraphOperations::ExportTasks(Asset.Get(), {Child, Leaf, Other}, Clipboard));
	const TArray<UTaskBase*> Imported = FTaskGraphOperations::ImportTasks(Destination.Get(), Clipboard, FVector2D(300, 200));
	if (!TestEqual(TEXT("复制重叠选择去重"), Imported.Num(), 2)) return false;
	UTaskBase* Copied = Imported[0];
	UTaskBase* CopiedOther = Imported[1];
	TestEqual(TEXT("复制保留层级"), Copied->SubTasks.Num(), 1);
	TestTrue(TEXT("复制所有 GUID 独立"), Copied->TaskGUID != Child->TaskGUID && Copied->SubTasks[0]->TaskGUID != Leaf->TaskGUID);
	TestEqual(TEXT("子节点归属目标资产"), Copied->SubTasks[0]->GetOuter(), static_cast<UObject*>(Destination.Get()));
	FString OriginalText, CopiedText;
	FTextStringHelper::WriteToBuffer(OriginalText, Child->TaskDescription);
	FTextStringHelper::WriteToBuffer(CopiedText, Copied->TaskDescription);
	TestEqual(TEXT("本地化文本身份保留"), CopiedText, OriginalText);
	TestTrue(TEXT("本地前置依赖重映射"), CopiedOther->Prerequisites[0].Asset.Get() == Destination.Get() && CopiedOther->Prerequisites[0].TaskGUID == Copied->TaskGUID);
	Undo();
	TestTrue(TEXT("跨资产粘贴撤销"), Destination->RootTasks.IsEmpty());
	Redo();
	TestEqual(TEXT("跨资产粘贴重做"), Destination->RootTasks.Num(), 2);
	TestTrue(TEXT("无关剪贴板不修改资产"), FTaskGraphOperations::ImportTasks(Destination.Get(), TEXT("{}"), FVector2D::ZeroVector).IsEmpty());

	View->SelectTasks({Child});
	FString PreviousClipboard;
	FPlatformApplicationMisc::ClipboardPaste(PreviousClipboard);
	ON_SCOPE_EXIT { FPlatformApplicationMisc::ClipboardCopy(*PreviousClipboard); };
	TestTrue(TEXT("右键复制命令"), Commands->ExecuteAction(FGenericCommands::Get().Copy.ToSharedRef()));
	TestTrue(TEXT("右键粘贴命令"), Commands->ExecuteAction(FGenericCommands::Get().Paste.ToSharedRef()));
	TestEqual(TEXT("同资产粘贴名称不冲突"), Asset->TaskMap.Num(), 6);
	Undo();
	TestEqual(TEXT("粘贴撤销"), Asset->TaskMap.Num(), 4);
	Redo();
	TestEqual(TEXT("粘贴重做"), Asset->TaskMap.Num(), 6);
	Undo();
	View->SelectTasks({Child});
	Commands->ExecuteAction(FGenericCommands::Get().Duplicate.ToSharedRef());
	TestEqual(TEXT("副本命令复制完整子树"), Asset->TaskMap.Num(), 6);
	Undo();
	TestEqual(TEXT("副本撤销"), Asset->TaskMap.Num(), 4);
	Redo();
	TestEqual(TEXT("副本重做"), Asset->TaskMap.Num(), 6);
	Undo();

	View->SelectTasks({Child});
	Commands->ExecuteAction(FGenericCommands::Get().Cut.ToSharedRef());
	TestEqual(TEXT("剪切删除子树"), Asset->TaskMap.Num(), 2);
	TestTrue(TEXT("删除清理本地前置依赖"), Other->Prerequisites.IsEmpty());
	Undo();
	TestTrue(TEXT("剪切撤销恢复依赖和子树"), Asset->TaskMap.Num() == 4 && Other->Prerequisites.Num() == 1);
	Redo();
	TestEqual(TEXT("剪切重做"), Asset->TaskMap.Num(), 2);
	Undo();
	View->SelectTasks({Leaf});
	Commands->ExecuteAction(FGenericCommands::Get().Delete.ToSharedRef());
	TestEqual(TEXT("删除命令"), Asset->TaskMap.Num(), 3);
	Undo();
	TestEqual(TEXT("删除撤销"), Leaf->ParentTask, Child);
	Redo();
	TestEqual(TEXT("删除重做"), Asset->TaskMap.Num(), 3);
	Undo();

	Schema->TryCreateConnection(Node(Other)->FindPinChecked(TEXT("Out")), Node(Leaf)->FindPinChecked(TEXT("In")));
	TestEqual(TEXT("连线修改父节点"), Leaf->ParentTask, Other);
	Undo();
	TestEqual(TEXT("连线撤销"), Leaf->ParentTask, Child);
	Redo();
	TestEqual(TEXT("连线重做"), Leaf->ParentTask, Other);
	Undo();
	Schema->BreakSinglePinLink(Node(Child)->FindPinChecked(TEXT("Out")), Node(Leaf)->FindPinChecked(TEXT("In")));
	TestNull(TEXT("断开单条线"), Leaf->ParentTask);
	Undo();
	TestEqual(TEXT("断线撤销"), Leaf->ParentTask, Child);
	Redo();
	TestNull(TEXT("断线重做"), Leaf->ParentTask);
	Undo();
	Schema->BreakNodeLinks(*Node(Child));
	TestTrue(TEXT("断开节点同时处理 In 和 Out"), !Child->ParentTask && !Leaf->ParentTask);
	Undo();
	TestTrue(TEXT("断开节点一步撤销完整关系"), Child->ParentTask == Root && Leaf->ParentTask == Child);
	Redo();
	TestTrue(TEXT("断开节点重做"), !Child->ParentTask && !Leaf->ParentTask);
	Undo();
	Schema->BreakPinLinks(*Node(Root)->FindPinChecked(TEXT("Out")), true);
	TestTrue(TEXT("断开 Out 保留子树"), !Child->ParentTask && !Other->ParentTask && Leaf->ParentTask == Child);
	Undo();
	TestTrue(TEXT("断开 Out 撤销"), Child->ParentTask == Root && Other->ParentTask == Root);
	Redo();
	TestNull(TEXT("断开 Out 重做"), Child->ParentTask);
	Undo();

	View->GetGraphEditor()->GetGraphPanel()->Update();
	const FVector2D BeforeMove = Other->GraphPosition;
	{
		const FScopedTransaction Transaction(NSLOCTEXT("TaskEditorTest", "Move", "Move Task Node"));
		SGraphNode::FNodeSet Filter;
		View->GetGraphEditor()->GetGraphPanel()->GetNodeWidgetFromGuid(Node(Other)->NodeGuid)->MoveTo(FVector2f(400, -80), Filter);
	}
	TestTrue(TEXT("实际 Slate 节点拖动写入资产并排序"), Other->GraphPosition.Y == -80 && Root->SubTasks[0] == Other);
	Undo();
	TestTrue(TEXT("拖动撤销恢复位置和执行顺序"), Other->GraphPosition == BeforeMove && Root->SubTasks[0] == Child);
	Redo();
	TestTrue(TEXT("拖动重做恢复位置和执行顺序"), Other->GraphPosition.Y == -80 && Root->SubTasks[0] == Other);
	Undo();
	const FVector2D BeforeLayout = Leaf->GraphPosition;
	FTaskGraphOperations::AutoLayout(Asset.Get()); View->Rebuild();
	TestTrue(TEXT("自动布局改变节点位置"), Leaf->GraphPosition != BeforeLayout);
	Undo();
	TestEqual(TEXT("自动布局撤销"), Leaf->GraphPosition, BeforeLayout);
	Redo();
	TestTrue(TEXT("自动布局重做"), Leaf->GraphPosition != BeforeLayout);
	Undo();

	View->SelectTasks({Child});
	FMenuBuilder NodeMenuBuilder(true, Commands);
	FActionMenuContent NodeMenu = View->CreateNodeMenu(Graph(), Node(Child), nullptr, &NodeMenuBuilder, false);
	FActionMenuContent BackgroundMenu = View->CreateActionMenu(Graph(), FVector2f::ZeroVector, {}, false, SGraphEditor::FActionMenuClosed());
	TFunction<FString(TSharedRef<SWidget>)> ReadText = [&](TSharedRef<SWidget> Widget)
	{
		FString Text;
		if (Widget->GetType() == FName("STextBlock")) Text += StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString() + TEXT("\n");
		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Index < Children->Num(); ++Index) Text += ReadText(Children->GetChildAt(Index));
		return Text;
	};
	const FString MenuText = ReadText(NodeMenu.Content);
	TestTrue(TEXT("节点右键保留类型与代码编辑"), MenuText.Contains(TEXT("Change Task Type")) && MenuText.Contains(TEXT("Go to Task Code")));
	TestFalse(TEXT("没有旧列表操作"), MenuText.Contains(TEXT("Move Up")) || MenuText.Contains(TEXT("Append")) || MenuText.Contains(TEXT("Insert")));
	TestTrue(TEXT("背景右键提供创建入口"), ReadText(BackgroundMenu.Content).Contains(TEXT("Create Task")));
	TestTrue(TEXT("状态栏显示当前选择与撤销说明"), ReadText(Editor->StatusWidget->TakeWidget()).Contains(TEXT("Ctrl+Z")));
	Asset->SourceObject = Destination.Get();
	TestFalse(TEXT("运行实例禁止修改"), View->CanEdit());
	TestFalse(TEXT("运行实例不能删除"), Commands->CanExecuteAction(FGenericCommands::Get().Delete.ToSharedRef()));
	TestNull(TEXT("运行实例不能新建"), FTaskGraphOperations::AddTask(Asset.Get(), UTaskBase::StaticClass(), FVector2D::ZeroVector));
	Asset->SourceObject = nullptr;

	FString Preview;
	if (FApp::CanEverRender() && FParse::Value(FCommandLine::Get(), TEXT("TaskPreview="), Preview))
	{
		View->GetGraphEditor()->GetGraphPanel()->Update();
		View->GetGraphEditor()->SetViewLocation(FVector2D(-40, -40), 0.9f);
		TSharedRef<SWidget> Layout = SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1)[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.74f)[View]
				+ SHorizontalBox::Slot().FillWidth(0.26f)[Editor->DetailsWidget->TakeWidget()]
			]
			+ SVerticalBox::Slot().AutoHeight()[Editor->StatusWidget->TakeWidget()];
		auto Render = [&](TSharedRef<SWidget> Widget, const FString& Path, FVector2D Size)
		{
			FWidgetRenderer Renderer(true);
			UTextureRenderTarget2D* Target = Renderer.DrawWidget(Widget, Size);
			FImage Image;
			if (TestTrue(TEXT("任务编辑器渲染"), FImageUtils::GetRenderTargetImage(Target, Image)))
			{
				if (Image.Format == ERawImageFormat::BGRA8) Image.GammaSpace = EGammaSpace::sRGB;
				TestTrue(TEXT("任务编辑器预览导出"), FImageUtils::SaveImageByExtension(*Path, Image));
			}
		};
		Render(Layout, Preview, FVector2D(1320, 680));
		Render(NodeMenu.Content, FPaths::GetPath(Preview) / TEXT("TaskNodeMenu.png"), FVector2D(340, 590));
		Render(BackgroundMenu.Content, FPaths::GetPath(Preview) / TEXT("TaskBackgroundMenu.png"), FVector2D(340, 550));
	}
	return true;
}

#endif
