#include "Procedure/Tests/ProcedureGraphTestTypes.h"

#include "Procedure/Base/ProcedureAsset.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/Graph/ProcedureAssetGraph.h"
#include "Procedure/Graph/ProcedureAssetGraphNode.h"
#include "Procedure/Graph/ProcedureAssetGraphSchema.h"
#include "Procedure/Graph/ProcedureGraphOperations.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "JsonObjectConverter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Linker.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ProcedureGraphTests
{
struct FFixture
{
    TStrongObjectPtr<UProcedureAsset> Asset;
    TStrongObjectPtr<UProcedureAssetGraph> Graph;
    TArray<FGuid> Ids;

    FFixture()
        : Asset(NewObject<UProcedureAsset>(GetTransientPackage(), NAME_None, RF_Transactional)),
          Graph(NewObject<UProcedureAssetGraph>(GetTransientPackage(), NAME_None, RF_Transactional))
    {
        Asset->StartId = FGuid::NewGuid();
        Graph->Asset = Asset.Get();
        Graph->Schema = UProcedureAssetGraphSchema::StaticClass();
        const TArray<UClass*> Classes = {UProcedureGraphTestNode::StaticClass(), UProcedureGraphSecondTestNode::StaticClass(),
            UProcedureGraphThirdTestNode::StaticClass(), UProcedureGraphDetachedTestNode::StaticClass()};
        for (int32 Index = 0; Index < Classes.Num(); ++Index)
        {
            UProcedureBase* Node = NewObject<UProcedureBase>(Asset.Get(), Classes[Index], NAME_None, RF_Transactional);
            Node->NodeId = FGuid::NewGuid();
            Node->GraphPosition = FVector2D(Index * 360, Index * 100);
            Asset->Nodes.Add(Node);
            Ids.Add(Node->NodeId);
        }
        FGuid Previous = Asset->StartId;
        for (int32 Index = 0; Index < 3; ++Index)
        {
            FProcedureLink Link;
            Link.From = Previous;
            Link.To = Ids[Index];
            Asset->Links.Add(Link);
            Previous = Ids[Index];
        }
        Asset->Recompile();
        Graph->Rebuild();
    }
};

struct FPersistencePackage
{
    const FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Root = TEXT("/ProcedurePersistence_") + Id + TEXT("/");
    const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/ProcedurePersistence") / Id) + TEXT("/");
    const FString PackageName = Root + TEXT("Procedure");
    const FString Filename = Directory / TEXT("Procedure.uasset");
    FAutomationTestBase& Test;
    bool bMounted = false;

    explicit FPersistencePackage(FAutomationTestBase& InTest)
        : Test(InTest)
    {
    }

    bool Create()
    {
        if (FindPackage(nullptr, *PackageName) || IFileManager::Get().DirectoryExists(*Directory) ||
            !IFileManager::Get().MakeDirectory(*Directory, true))
        {
            Test.AddError(TEXT("无法创建独立流程保存测试目录。"));
            return false;
        }
        FPackageName::RegisterMountPoint(Root, Directory);
        bMounted = true;
        return true;
    }

    void Release(UPackage* InPackage) const
    {
        if (!InPackage) return;
        if (UProcedureAsset* Asset = FindObject<UProcedureAsset>(InPackage, TEXT("Procedure")))
        {
            Asset->ClearFlags(RF_Standalone);
        }
        InPackage->ClearFlags(RF_Standalone);
        InPackage->SetDirtyFlag(false);
        ResetLoaders(InPackage);
        CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    }

    ~FPersistencePackage()
    {
        if (!bMounted) return;
        Release(FindPackage(nullptr, *PackageName));
        FPackageName::UnRegisterMountPoint(Root, Directory);
        const bool bDeleted = !IFileManager::Get().FileExists(*Filename) || IFileManager::Get().Delete(*Filename, false, true);
        Test.TestTrue(TEXT("删除本测试创建的包文件"), bDeleted);
        Test.TestTrue(TEXT("只删除本测试的空目录"), IFileManager::Get().DeleteDirectory(*Directory, false, false));
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcedureReachabilityTest, "WHFramework.Procedure.Graph.ReachabilityAndSwitching",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProcedureReachabilityTest::RunTest(const FString& Parameters)
{
    ProcedureGraphTests::FFixture Fixture;
    UProcedureGraphTestNode::Initialized = 0;
    UProcedureGraphTestNode::Entered = 0;
    UProcedureGraphTestNode::Shutdown = 0;
    TStrongObjectPtr<UProcedureModule> Module(NewObject<UProcedureModule>());
    Module->SetDefaultAsset(Fixture.Asset.Get());
    TestTrue(TEXT("Start begins the compiled flow"), Module->StartMain());
    TestEqual(TEXT("Only three reachable definitions initialize"), UProcedureGraphTestNode::Initialized, 3);
    TestEqual(TEXT("Detached node has no runtime object"), Module->GetProcedureById(Fixture.Ids[3]), static_cast<UProcedureBase*>(nullptr));
    TestFalse(TEXT("ByClass cannot enter detached nodes"), Module->SwitchProcedureByClass<UProcedureGraphDetachedTestNode>());
    TestFalse(TEXT("ByIndex does not enumerate editor-only detached nodes"), Module->SwitchProcedureByIndex(3));
    TestFalse(TEXT("Null class is rejected"), Module->SwitchProcedureByClass(nullptr));
    TestFalse(TEXT("Invalid index does not stop the current node"), Module->SwitchProcedureByIndex(INDEX_NONE));
    TestTrue(TEXT("A direct class switch may skip intermediate reachable nodes"), Module->SwitchProcedureByClass<UProcedureGraphThirdTestNode>());
    TestTrue(TEXT("Pre follows the default link, not direct-switch history"), Module->SwitchPreProcedure());
    TestTrue(TEXT("Pre selected the second node"), Module->IsCurrentProcedureIndex(1));
    TestTrue(TEXT("Next follows the explicit successor"), Module->SwitchNextProcedure());
    TestFalse(TEXT("A disconnected endpoint has no Next fallback"), Module->SwitchNextProcedure());
    TestFalse(TEXT("Ambiguous base-class lookup cannot choose an arbitrary node"), Module->SwitchProcedureByClass<UProcedureGraphTestNode>());
    TestTrue(TEXT("First restores Start's business successor"), Module->SwitchFirstProcedure());
    const FProcedureActivationToken Old = Module->CaptureActivation();
    UProcedureBase* First = Module->GetCurrentProcedure();
    UProcedureBase* Second = Module->GetProcedureByIndex(1);
    Module->SwitchProcedure(Second);
    Module->SwitchProcedure(First);
    TestFalse(TEXT("Old callback is rejected after the same object is reentered"), Module->SwitchProcedureFrom(Old, Second));
    TestTrue(TEXT("Rejected callback preserves current node"), Module->IsCurrentProcedure(First));
    TestFalse(TEXT("Author template pointer is not a runtime member"), Module->SwitchProcedure(Fixture.Asset->Nodes[1]));
    Module->StopMain();
    TestEqual(TEXT("All three initialized nodes are explicitly shut down"), UProcedureGraphTestNode::Shutdown, 3);
    TestTrue(TEXT("Asset template was never entered"), Fixture.Asset->Nodes[0]->GetProcedureState() == EProcedureState::None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcedureDispatchTest, "WHFramework.Procedure.Graph.DeferredDispatchAndStop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProcedureDispatchTest::RunTest(const FString& Parameters)
{
    ProcedureGraphTests::FFixture Fixture;
    for (UProcedureBase* Node : Fixture.Asset->Nodes)
    {
        CastChecked<UProcedureGraphTestNode>(Node)->bAdvanceOnEnter = true;
    }
    UProcedureGraphTestNode::Entered = 0;
    TStrongObjectPtr<UProcedureModule> Module(NewObject<UProcedureModule>());
    Module->SetDefaultAsset(Fixture.Asset.Get());
    TestTrue(TEXT("Start commits one entry"), Module->StartMain());
    TestEqual(TEXT("Automatic Next from Enter is queued"), UProcedureGraphTestNode::Entered, 1);
    Module->OnRefresh(0.f, false);
    TestEqual(TEXT("One queued transition is consumed per frame"), UProcedureGraphTestNode::Entered, 2);
    Module->OnRefresh(0.f, false);
    TestEqual(TEXT("Next frame reaches the third node"), UProcedureGraphTestNode::Entered, 3);
    Module->OnRefresh(0.f, false);
    TestEqual(TEXT("Endpoint does not loop or run detached templates"), UProcedureGraphTestNode::Entered, 3);
    Module->StopMain();
    for (UProcedureBase* Node : Fixture.Asset->Nodes)
    {
        CastChecked<UProcedureGraphTestNode>(Node)->bAdvanceOnEnter = false;
    }
    CastChecked<UProcedureGraphTestNode>(Fixture.Asset->Nodes[0])->bStopOnLeave = true;
    Module->StartMain();
    const int32 EntriesBefore = UProcedureGraphTestNode::Entered;
    Module->SwitchNextProcedure();
    TestNull(TEXT("Stop requested during Leave is not lost"), Module->GetCurrentAsset());
    TestEqual(TEXT("Deferred stop prevents the next Enter"), UProcedureGraphTestNode::Entered, EntriesBefore);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcedureInitializeStopTest, "WHFramework.Procedure.Runtime.StopDuringInitialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProcedureInitializeStopTest::RunTest(const FString& Parameters)
{
    ProcedureGraphTests::FFixture Fixture;
    CastChecked<UProcedureGraphTestNode>(Fixture.Asset->Nodes[0])->bStopOnInitialize = true;
    UProcedureGraphTestNode::Initialized = 0;
    const int32 EnteredBefore = UProcedureGraphTestNode::Entered;
    const int32 ShutdownBefore = UProcedureGraphTestNode::Shutdown;
    TStrongObjectPtr<UProcedureModule> Module(NewObject<UProcedureModule>());
    Module->SetDefaultAsset(Fixture.Asset.Get());
    TestFalse(TEXT("Stop during initialization aborts start"), Module->StartMain());
    TestEqual(TEXT("No later initialization callbacks after stop"), UProcedureGraphTestNode::Initialized, 1);
    TestEqual(TEXT("Aborted start emits no enter"), UProcedureGraphTestNode::Entered, EnteredBefore);
    TestEqual(TEXT("Only initialized nodes receive shutdown"), UProcedureGraphTestNode::Shutdown, ShutdownBefore + 1);
    TestNull(TEXT("Aborted start releases runtime asset"), Module->GetCurrentAsset());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcedureCompileFailureTest, "WHFramework.Procedure.Graph.CompileFailuresClearStaleProgram",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProcedureCompileFailureTest::RunTest(const FString& Parameters)
{
    ProcedureGraphTests::FFixture Fixture;
    FProcedureLink Cycle;
    Cycle.From = Fixture.Ids[2];
    Cycle.To = Fixture.Ids[0];
    Fixture.Asset->Links.Add(Cycle);
    TestFalse(TEXT("Reachable default cycles are rejected"), Fixture.Asset->Recompile());
    TestTrue(TEXT("Failed compile clears previous compiled nodes"), Fixture.Asset->CompiledNodes.IsEmpty());
    TestFalse(TEXT("Failed compile clears entry"), Fixture.Asset->EntryNodeId.IsValid());
    Fixture.Asset->Links.Pop();
    Fixture.Asset->Recompile();
    Fixture.Asset->Links.RemoveAt(1);
    TestTrue(TEXT("A cut chain remains a valid partial flow"), Fixture.Asset->Recompile());
    TestEqual(TEXT("Only prefix reachable from Start compiles"), Fixture.Asset->CompiledNodes.Num(), 1);
    Fixture.Asset->Links.Reset();
    TestTrue(TEXT("An unconnected Start compiles an empty flow"), Fixture.Asset->Recompile());
    TestTrue(TEXT("No implicit first business node fallback"), Fixture.Asset->CompiledNodes.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcedureEditorTransactionTest, "WHFramework.Procedure.Graph.TransactionsAndClipboardValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProcedureEditorTransactionTest::RunTest(const FString& Parameters)
{
    if (!GEditor || GEditor->PlayWorld)
    {
        AddError(TEXT("Run this editor test outside PIE."));
        return false;
    }
    ProcedureGraphTests::FFixture Fixture;
    TestFalse(TEXT("Occupied ports must be explicitly disconnected first"), FProcedureGraphOperations::CanConnect(Fixture.Asset.Get(), Fixture.Ids[0], TEXT("Out"), Fixture.Ids[2]));
    TestTrue(TEXT("Disconnect edits model and compiled output in one transaction"), FProcedureGraphOperations::Disconnect(Fixture.Graph.Get(), Fixture.Ids[0], TEXT("Out")));
    TestEqual(TEXT("Cut updates reachability immediately"), Fixture.Asset->CompiledNodes.Num(), 1);
    GEditor->UndoTransaction();
    Fixture.Graph->Rebuild();
    TestEqual(TEXT("Undo restores the compiled prefix and actual model links"), Fixture.Asset->CompiledNodes.Num(), 3);
    TestNotNull(TEXT("Visual nodes are re-resolved by GUID after undo"), Fixture.Graph->FindNode(Fixture.Ids[1]));
    GEditor->RedoTransaction();
    Fixture.Graph->Rebuild();
    TestEqual(TEXT("Redo removes the suffix again"), Fixture.Asset->CompiledNodes.Num(), 1);
    GEditor->UndoTransaction();
    Fixture.Graph->Rebuild();
    FString Clipboard;
    TestTrue(TEXT("Export uses model serialization without the OS clipboard"), FProcedureGraphOperations::Export(Fixture.Asset.Get(), {Fixture.Ids[0], Fixture.Ids[1]}, Clipboard));
    TSharedPtr<FJsonObject> Document;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Clipboard), Document) || !Document)
    {
        AddError(TEXT("Export did not produce valid clipboard JSON."));
        return false;
    }
    FProcedureLink Invalid;
    Invalid.From = Fixture.Ids[1];
    Invalid.To = Fixture.Ids[0];
    TSharedRef<FJsonObject> InvalidObject = MakeShared<FJsonObject>();
    FJsonObjectConverter::UStructToJsonObject(FProcedureLink::StaticStruct(), &Invalid, InvalidObject, 0, 0);
    TArray<TSharedPtr<FJsonValue>> Links = Document->GetArrayField(TEXT("Links"));
    Links.Add(MakeShared<FJsonValueObject>(InvalidObject));
    Document->SetArrayField(TEXT("Links"), Links);
    FString InvalidClipboard;
    FJsonSerializer::Serialize(Document.ToSharedRef(), TJsonWriterFactory<>::Create(&InvalidClipboard));
    const int32 CountBefore = Fixture.Asset->Nodes.Num();
    TestTrue(TEXT("Malformed cyclic paste is rejected before any model mutation"), FProcedureGraphOperations::Import(Fixture.Graph.Get(), InvalidClipboard, FVector2D::ZeroVector).IsEmpty());
    TestEqual(TEXT("Rejected paste leaves author nodes unchanged"), Fixture.Asset->Nodes.Num(), CountBefore);
    const TArray<FGuid> Added = FProcedureGraphOperations::Import(Fixture.Graph.Get(), Clipboard, FVector2D(1000, 500));
    TestEqual(TEXT("Valid pasted nodes receive new stable identities"), Added.Num(), 2);
    if (Added.Num() != 2)
    {
        return false;
    }
    TestFalse(TEXT("Pasted fragment stays disconnected until explicitly wired"), Fixture.Asset->CompiledNodes.Contains(Fixture.Asset->FindNode(Added[0])));
    TestTrue(TEXT("Pasted nodes do not reuse source GUIDs"), Added[0] != Fixture.Ids[0] && Added[1] != Fixture.Ids[1]);
    GEditor->UndoTransaction();
    Fixture.Graph->Rebuild();
    TestEqual(TEXT("Undo paste removes cloned author definitions"), Fixture.Asset->Nodes.Num(), CountBefore);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcedureStartInitializationTest, "WHFramework.Procedure.Graph.StartInitializationAndVisibleLinks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProcedureStartInitializationTest::RunTest(const FString& Parameters)
{
    if (!GEditor || GEditor->PlayWorld) return false;
    ProcedureGraphTests::FFixture Fixture;
    Fixture.Asset->Links.Reset();
    Fixture.Asset->StartId.Invalidate();
    TestFalse(TEXT("缺少起点身份不能编译"), Fixture.Asset->Recompile());
    TestTrue(TEXT("通过事务显式初始化空连线图起点"), FProcedureGraphOperations::InitializeStart(Fixture.Graph.Get()));
    const FGuid Start = Fixture.Asset->StartId;
    TestTrue(TEXT("起点身份有效"), Start.IsValid());
    TestFalse(TEXT("有效起点不能重复初始化"), FProcedureGraphOperations::InitializeStart(Fixture.Graph.Get()));
    TestTrue(TEXT("起点到业务节点建立真实连线"), FProcedureGraphOperations::Connect(Fixture.Graph.Get(), Start, TEXT("Out"), Fixture.Ids[0]));
    Fixture.Graph->Rebuild();
    UEdGraphNode* StartNode = Fixture.Graph->FindNode(Start);
    UEdGraphNode* BusinessNode = Fixture.Graph->FindNode(Fixture.Ids[0]);
    UEdGraphPin* Output = StartNode ? StartNode->FindPin(TEXT("Out"), EGPD_Output) : nullptr;
    UEdGraphPin* Input = BusinessNode ? BusinessNode->FindPin(TEXT("In"), EGPD_Input) : nullptr;
    TestTrue(TEXT("图重建后两端Pin保留可绘制连接"), Output && Input && Output->LinkedTo.Contains(Input));
    TestFalse(TEXT("已有连接不能再次提示可连接"), FProcedureGraphOperations::CanConnect(Fixture.Asset.Get(), Start, TEXT("Out"), Fixture.Ids[0]));
    TestEqual(TEXT("编译入口来自真实Start连线"), Fixture.Asset->EntryNodeId, Fixture.Ids[0]);
    GEditor->UndoTransaction();
    Fixture.Graph->Rebuild();
    TestTrue(TEXT("撤销连线恢复空程序"), Fixture.Asset->CompiledNodes.IsEmpty());
    GEditor->UndoTransaction();
    Fixture.Graph->Rebuild();
    TestFalse(TEXT("撤销初始化恢复缺失身份"), Fixture.Asset->StartId.IsValid());
    GEditor->RedoTransaction();
    Fixture.Graph->Rebuild();
    TestEqual(TEXT("重做恢复同一起点身份"), Fixture.Asset->StartId, Start);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcedureCollectableTextPersistenceTest, "WHFramework.Procedure.Graph.CollectableTextSaveAndReload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProcedureCollectableTextPersistenceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    if (!GEditor || GEditor->PlayWorld)
    {
        AddError(TEXT("流程包保存测试需要在非 PIE 编辑器中执行。"));
        return false;
    }

    ProcedureGraphTests::FPersistencePackage Temporary(*this);
    if (!Temporary.Create()) return false;
    UPackage* Package = CreatePackage(*Temporary.PackageName);
    UProcedureAsset* Asset = NewObject<UProcedureAsset>(Package, TEXT("Procedure"), RF_Public | RF_Standalone);
    Asset->StartId = FGuid::NewGuid();
    const FGuid StartId = Asset->StartId;
    UProcedureBase* First = NewObject<UProcedureBase>(Asset, TEXT("First"));
    UProcedureBase* Second = NewObject<UProcedureBase>(Asset, TEXT("Second"));
    First->NodeId = FGuid::NewGuid();
    Second->NodeId = FGuid::NewGuid();
    const FGuid FirstId = First->NodeId;
    const FGuid SecondId = Second->NodeId;
    First->ProcedureDisplayName = NSLOCTEXT("ProcedurePersistenceTest", "FirstTitle", "流程保存测试节点");
    First->ProcedureDescription = NSLOCTEXT("ProcedurePersistenceTest", "FirstDescription", "可收集的流程说明文本。");
    Second->ProcedureDisplayName = NSLOCTEXT("ProcedurePersistenceTest", "SecondTitle", "后续流程节点");
    First->GraphPosition = FVector2D(125.0, -64.0);
    Second->GraphPosition = FVector2D(430.0, 75.0);
    Asset->Nodes = {First, Second};
    FProcedureLink Entry;
    Entry.From = StartId;
    Entry.To = FirstId;
    FProcedureLink Next;
    Next.From = FirstId;
    Next.To = SecondId;
    Asset->Links = {Entry, Next};
    if (!TestTrue(TEXT("节点文字参与本地化收集"), First->ProcedureDisplayName.ShouldGatherForLocalization() &&
        First->ProcedureDescription.ShouldGatherForLocalization()) ||
        !TestTrue(TEXT("保存前真实流程连接可编译"), Asset->Recompile())) return false;

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    if (!TestTrue(TEXT("含可收集文字的流程包实际写入磁盘"), UPackage::SavePackage(Package, Asset, *Temporary.Filename, SaveArgs))) return false;
    if (!TestTrue(TEXT("流程包文件存在且非空"), IFileManager::Get().FileSize(*Temporary.Filename) > 0)) return false;
    const TWeakObjectPtr<UPackage> OriginalPackage(Package);
    Temporary.Release(Package);
    Package = nullptr;
    Asset = nullptr;
    First = nullptr;
    Second = nullptr;
    if (!TestFalse(TEXT("重载前原流程包已被回收"), OriginalPackage.IsValid()) ||
        !TestNull(TEXT("重载不能复用旧内存包"), FindPackage(nullptr, *Temporary.PackageName))) return false;

    UPackage* ReloadedPackage = LoadPackage(nullptr, *Temporary.PackageName, LOAD_None);
    UProcedureAsset* Reloaded = ReloadedPackage ? FindObject<UProcedureAsset>(ReloadedPackage, TEXT("Procedure")) : nullptr;
    if (!TestNotNull(TEXT("从磁盘重新加载流程资产"), Reloaded)) return false;
    TestEqual(TEXT("起点身份保存"), Reloaded->StartId, StartId);
    if (!TestEqual(TEXT("两个作者节点保留"), Reloaded->Nodes.Num(), 2) ||
        !TestEqual(TEXT("两个作者连接保留"), Reloaded->Links.Num(), 2)) return false;
    UProcedureBase* ReloadedFirst = Reloaded->FindNode(FirstId);
    UProcedureBase* ReloadedSecond = Reloaded->FindNode(SecondId);
    if (!TestNotNull(TEXT("第一个节点按身份恢复"), ReloadedFirst) ||
        !TestNotNull(TEXT("第二个节点按身份恢复"), ReloadedSecond)) return false;
    TestTrue(TEXT("加载后文字仍可收集"), ReloadedFirst->ProcedureDisplayName.ShouldGatherForLocalization() &&
        ReloadedFirst->ProcedureDescription.ShouldGatherForLocalization());
    const FString* TitleSource = FTextInspector::GetSourceString(ReloadedFirst->ProcedureDisplayName);
    const FString* DescriptionSource = FTextInspector::GetSourceString(ReloadedFirst->ProcedureDescription);
    TestTrue(TEXT("标题源文本保留"), TitleSource && *TitleSource == TEXT("流程保存测试节点"));
    TestTrue(TEXT("说明源文本保留"), DescriptionSource && *DescriptionSource == TEXT("可收集的流程说明文本。"));
    TestEqual(TEXT("节点位置保留"), ReloadedFirst->GraphPosition, FVector2D(125.0, -64.0));
    TestTrue(TEXT("原始两条连接保留"), Reloaded->Links[0].From == StartId && Reloaded->Links[0].To == FirstId &&
        Reloaded->Links[0].Output == TEXT("Out") && Reloaded->Links[1].From == FirstId && Reloaded->Links[1].To == SecondId &&
        Reloaded->Links[1].Output == TEXT("Out"));
    FString Error;
    TestTrue(TEXT("保存后的运行流程有效"), Reloaded->ValidateCompiled(Error));
    TestTrue(TEXT("运行流程按连接顺序保留"), Reloaded->EntryNodeId == FirstId && Reloaded->CompiledNodes.Num() == 2 &&
        Reloaded->CompiledNodes[0] == ReloadedFirst && Reloaded->CompiledNodes[1] == ReloadedSecond);
    TStrongObjectPtr<UProcedureAssetGraph> Graph(NewObject<UProcedureAssetGraph>(GetTransientPackage()));
    Graph->Asset = Reloaded;
    Graph->Schema = UProcedureAssetGraphSchema::StaticClass();
    Graph->Rebuild();
    UEdGraphNode* FirstVisual = Graph->FindNode(FirstId);
    UEdGraphNode* SecondVisual = Graph->FindNode(SecondId);
    UEdGraphPin* Output = FirstVisual ? FirstVisual->FindPin(TEXT("Out"), EGPD_Output) : nullptr;
    UEdGraphPin* Input = SecondVisual ? SecondVisual->FindPin(TEXT("In"), EGPD_Input) : nullptr;
    TestTrue(TEXT("重建的图形端口连接保留"), Output && Input && Output->LinkedTo.Contains(Input));
    return true;
}
#endif
