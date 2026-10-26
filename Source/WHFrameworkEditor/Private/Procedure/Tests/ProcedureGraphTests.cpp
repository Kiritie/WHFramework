#include "Procedure/Tests/ProcedureGraphTestTypes.h"

#include "Procedure/Base/ProcedureAsset.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/Graph/ProcedureAssetGraph.h"
#include "Procedure/Graph/ProcedureAssetGraphNode.h"
#include "Procedure/Graph/ProcedureAssetGraphSchema.h"
#include "Procedure/Graph/ProcedureGraphOperations.h"
#include "Misc/AutomationTest.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "JsonObjectConverter.h"
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
#endif
