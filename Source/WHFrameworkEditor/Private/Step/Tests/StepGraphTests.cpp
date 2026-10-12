#include "Step/Tests/StepGraphTestTypes.h"

#include "Step/Base/StepAsset.h"
#include "Step/Base/StepRuntime.h"
#include "Step/Asset/StepAssetFactory.h"
#include "Step/Graph/StepAssetGraph.h"
#include "Step/Graph/StepAssetGraphNode.h"
#include "Step/Graph/StepAssetGraphSchema.h"
#include "Step/Graph/StepGraphOperations.h"
#include "Misc/AutomationTest.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace StepGraphTests
{
	UStepGraphTestStep* Add(UStepAsset* Asset, float X, float Y)
	{
		UStepGraphTestStep* Step = NewObject<UStepGraphTestStep>(Asset, NAME_None, RF_Transactional);
		Step->StepGUID = FGuid::NewGuid().ToString();
		Step->GraphPosition = FVector2D(X, Y);
		Asset->Steps.Add(Step);
		return Step;
	}

	void Link(UStepAsset* Asset, const FString& Parent, UStepBase* Child)
	{
		FStepGraphLink Edge;
		Edge.ParentGUID = Parent;
		Edge.ChildGUID = Child->StepGUID;
		Asset->Links.Add(Edge);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStepHierarchyCompileTest, "WHFramework.Step.Graph.HierarchyAndReachability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStepHierarchyCompileTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UStepAsset> Asset(NewObject<UStepAsset>());
	UStepBase* LowerRoot = StepGraphTests::Add(Asset.Get(), 340, 100);
	UStepBase* UpperRoot = StepGraphTests::Add(Asset.Get(), 340, -50);
	UStepBase* VisuallyHighestChild = StepGraphTests::Add(Asset.Get(), 680, -10000);
	UStepBase* OtherChild = StepGraphTests::Add(Asset.Get(), 680, 0);
	UStepBase* UpperRootChild = StepGraphTests::Add(Asset.Get(), 680, 999);
	UStepBase* Detached = StepGraphTests::Add(Asset.Get(), 0, -50000);
	StepGraphTests::Link(Asset.Get(), Asset->StartGUID, LowerRoot);
	StepGraphTests::Link(Asset.Get(), Asset->StartGUID, UpperRoot);
	StepGraphTests::Link(Asset.Get(), LowerRoot->StepGUID, OtherChild);
	StepGraphTests::Link(Asset.Get(), LowerRoot->StepGUID, VisuallyHighestChild);
	StepGraphTests::Link(Asset.Get(), UpperRoot->StepGUID, UpperRootChild);
	TArray<FText> Errors;
	if(!TestTrue(TEXT("Compile"), Asset->Compile(Errors)))
	{
		return false;
	}
	TestEqual(TEXT("Only reachable nodes"), Asset->CompiledGraph.Nodes.Num(), 5);
	TestEqual(TEXT("Upper root first"), Asset->CompiledGraph.Nodes[0].StepGUID, UpperRoot->StepGUID);
	TestEqual(TEXT("Upper subtree stays together"), Asset->CompiledGraph.Nodes[1].StepGUID, UpperRootChild->StepGUID);
	TestEqual(TEXT("Lower root follows whole subtree"), Asset->CompiledGraph.Nodes[2].StepGUID, LowerRoot->StepGUID);
	TestEqual(TEXT("Direct children sort locally"), Asset->CompiledGraph.Nodes[3].StepGUID, VisuallyHighestChild->StepGUID);
	TestEqual(TEXT("Depth preserved"), Asset->CompiledGraph.Nodes[3].Hierarchy, 1);
	TStrongObjectPtr<UStepGraphTestModule> Module(NewObject<UStepGraphTestModule>());
	if(!TestTrue(TEXT("Build runtime"), Module->Prepare(Asset.Get())))
	{
		return false;
	}
	TestNull(TEXT("Detached has no runtime"), Module->GetRuntime(Asset.Get())->FindStep(Detached->StepGUID));
	TestEqual(TEXT("Template never initializes"), CastChecked<UStepGraphTestStep>(LowerRoot)->InitializeCount, 0);
	TestEqual(TEXT("Runtime initializes once"), CastChecked<UStepGraphTestStep>(Module->GetStepByGUID(LowerRoot->StepGUID))->InitializeCount, 1);
	TStrongObjectPtr<UStepAsset> TieAsset(NewObject<UStepAsset>());
	UStepBase* TieLeft = StepGraphTests::Add(TieAsset.Get(), 340, 20);
	UStepBase* TieRight = StepGraphTests::Add(TieAsset.Get(), 340, 20);
	StepGraphTests::Link(TieAsset.Get(), TieAsset->StartGUID, TieLeft);
	StepGraphTests::Link(TieAsset.Get(), TieAsset->StartGUID, TieRight);
	if(!TestTrue(TEXT("Equal coordinates compile"), TieAsset->Compile(Errors)))
	{
		return false;
	}
	TestEqual(TEXT("GUID breaks coordinate tie"), TieAsset->CompiledGraph.RootGUIDs[0], TieLeft->StepGUID < TieRight->StepGUID ? TieLeft->StepGUID : TieRight->StepGUID);
	FGuid Parsed;
	FGuid::Parse(TieLeft->StepGUID, Parsed);
	TieRight->StepGUID = Parsed.ToString(EGuidFormats::DigitsWithHyphensInBraces);
	TestFalse(TEXT("Alternate textual form of same GUID rejected"), TieAsset->Compile(Errors));
	StepGraphTests::Link(Asset.Get(), UpperRoot->StepGUID, OtherChild);
	TestFalse(TEXT("Multi parent rejected"), Asset->Compile(Errors));
	TestEqual(TEXT("No stale graph after error"), Asset->CompiledGraph.Nodes.Num(), 0);
	Asset->Links.Pop();
	const uint64 InfinityBits = 0x7ff0000000000000ULL;
	FMemory::Memcpy(&Asset->StartPosition.X, &InfinityBits, sizeof(InfinityBits));
	TestFalse(TEXT("Non-finite Start position rejected"), Asset->Compile(Errors));
	TestEqual(TEXT("Invalid Start clears compiled output"), Asset->CompiledGraph.Nodes.Num(), 0);
	Asset->StartPosition = FVector2D::ZeroVector;
	FStepGraphLink Cycle;
	Cycle.ParentGUID = OtherChild->StepGUID;
	Cycle.ChildGUID = LowerRoot->StepGUID;
	Asset->Links.RemoveAll([&](const FStepGraphLink& Edge){ return Edge.ChildGUID == LowerRoot->StepGUID; });
	Asset->Links.Add(Cycle);
	TestFalse(TEXT("Detached cycle rejected"), Asset->Compile(Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStepEditorTransactionTest, "WHFramework.Step.Graph.TransactionsAndClipboard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStepEditorTransactionTest::RunTest(const FString& Parameters)
{
	if(!TestNotNull(TEXT("Editor required"), GEditor) || !TestNull(TEXT("Close PIE before transaction test"), GEditor->PlayWorld))
	{
		return false;
	}
	TStrongObjectPtr<UStepAsset> Asset(NewObject<UStepAsset>(GetTransientPackage(), NAME_None, RF_Transactional));
	UStepBase* Parent = FStepGraphOperations::AddStep(Asset.Get(), UStepGraphTestStep::StaticClass(), FVector2D(340, 0), Asset->StartGUID);
	if(!TestNotNull(TEXT("Create parent"), Parent))
	{
		return false;
	}
	UStepBase* Child = FStepGraphOperations::AddStep(Asset.Get(), UStepGraphTestStep::StaticClass(), FVector2D(680, 100), Parent->StepGUID);
	if(!TestNotNull(TEXT("Create child"), Child))
	{
		return false;
	}
	const FString ParentGUID = Parent->StepGUID;
	const FString ChildGUID = Child->StepGUID;
	TStrongObjectPtr<UStepAssetGraph> Graph(NewObject<UStepAssetGraph>(GetTransientPackage(), NAME_None, RF_Transient | RF_Transactional));
	Graph->StepAsset = Asset.Get();
	Graph->Schema = UStepAssetGraphSchema::StaticClass();
	Graph->Rebuild();
	TestFalse(TEXT("Start cannot be deleted"), Graph->FindNode(Asset->StartGUID)->CanUserDeleteNode());
	TestFalse(TEXT("Start cannot be duplicated"), Graph->FindNode(Asset->StartGUID)->CanDuplicateNode());
	TestTrue(TEXT("Disconnect exact edge"), FStepGraphOperations::DisconnectEdge(Asset.Get(), ParentGUID, ChildGUID));
	TestEqual(TEXT("Detached still authored"), Asset->Steps.Num(), 2);
	TestEqual(TEXT("Detached excluded"), Asset->CompiledGraph.Nodes.Num(), 1);
	GEditor->UndoTransaction();
	Graph->Rebuild();
	TestEqual(TEXT("Undo model edge"), Asset->CompiledGraph.Nodes.Num(), 2);
	UStepAssetGraphNode* RestoredNode = Graph->FindNode(ChildGUID);
	if(!TestNotNull(TEXT("Reacquire by stable identity"), RestoredNode) || !TestNotNull(TEXT("Reacquire input"), RestoredNode->FindPin(TEXT("In"))))
	{
		return false;
	}
	TestEqual(TEXT("Undo graph edge"), RestoredNode->FindPin(TEXT("In"))->LinkedTo.Num(), 1);
	GEditor->RedoTransaction();
	Graph->Rebuild();
	TestEqual(TEXT("Redo model edge"), Asset->CompiledGraph.Nodes.Num(), 1);
	FString Serialized;
	TestTrue(TEXT("Export without system clipboard"), FStepGraphOperations::ExportSteps(Asset.Get(), {Asset->FindDefinition(ParentGUID), Asset->FindDefinition(ChildGUID)}, Serialized));
	const TArray<UStepBase*> Pasted = FStepGraphOperations::ImportSteps(Asset.Get(), Serialized, FVector2D(1000, 500));
	TestEqual(TEXT("Paste count"), Pasted.Num(), 2);
	if(Pasted.Num() != 2)
	{
		return false;
	}
	TestNotEqual(TEXT("Paste regenerates GUID"), Pasted[0]->StepGUID, ParentGUID);
	TestEqual(TEXT("Paste is disconnected from Start"), Asset->CompiledGraph.Nodes.Num(), 1);
	const int32 Count = Asset->Steps.Num();
	TestEqual(TEXT("Invalid import rejected"), FStepGraphOperations::ImportSteps(Asset.Get(), TEXT("{}"), FVector2D::ZeroVector).Num(), 0);
	TestEqual(TEXT("Invalid import atomic"), Asset->Steps.Num(), Count);
	GEditor->UndoTransaction();
	TestEqual(TEXT("Paste undo includes author objects"), Asset->Steps.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStepRuntimeInvariantTest, "WHFramework.Step.Runtime.SkipPauseSaveReentrancy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStepRuntimeInvariantTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UStepAsset> Asset(NewObject<UStepAsset>());
	UStepGraphTestStep* RootTemplate = StepGraphTests::Add(Asset.Get(), 340, 0);
	UStepGraphTestStep* ChildTemplate = StepGraphTests::Add(Asset.Get(), 680, 0);
	ChildTemplate->bAllowSkip = false;
	ChildTemplate->StepExecuteType = EStepExecuteType::Procedure;
	StepGraphTests::Link(Asset.Get(), Asset->StartGUID, RootTemplate);
	StepGraphTests::Link(Asset.Get(), RootTemplate->StepGUID, ChildTemplate);
	TStrongObjectPtr<UStepGraphTestModule> Module(NewObject<UStepGraphTestModule>());
	if(!TestTrue(TEXT("Prepare"), Module->Prepare(Asset.Get())))
	{
		return false;
	}
	Module->StartStep();
	UStepGraphTestStep* Root = CastChecked<UStepGraphTestStep>(Module->GetStepByGUID(RootTemplate->StepGUID));
	UStepGraphTestStep* Child = CastChecked<UStepGraphTestStep>(Module->GetStepByGUID(ChildTemplate->StepGUID));
	Module->ExecuteStep(Root);
	TestEqual(TEXT("Child entered"), Child->StepState, EStepState::Entered);
	Module->CompleteStep(Root, EStepExecuteResult::Succeed);
	TestEqual(TEXT("Parent cannot pass unskippable child"), Root->StepState, EStepState::Executing);
	TestEqual(TEXT("No premature parent completion event"), Root->CompleteCount, 0);
	Module->OnPause();
	Module->OnRefresh(20.f, false);
	TestEqual(TEXT("Paused child does not execute"), Child->ExecuteCount, 0);
	FStepModuleSaveData Save = Module->ToData().GetRef<FStepModuleSaveData>();
	Save.MakeSaved();
	TStrongObjectPtr<UStepGraphTestModule> Restored(NewObject<UStepGraphTestModule>());
	Restored->LoadData(FParameter(Save), EPhase::Primary);
	UStepRuntime* PrimaryRuntime = Restored->GetRuntime(Asset.Get());
	Restored->LoadData(FParameter(Save), EPhase::Primary);
	TestEqual(TEXT("Repeated Primary does not replace pending runtime"), Restored->GetRuntime(Asset.Get()), PrimaryRuntime);
	Restored->LoadData(FParameter(Save), EPhase::Final);
	UStepGraphTestStep* RestoredRoot = Cast<UStepGraphTestStep>(Restored->GetStepByGUID(RootTemplate->StepGUID));
	if(!TestNotNull(TEXT("Restore stable node"), RestoredRoot))
	{
		return false;
	}
	TestEqual(TEXT("Restore does not replay enter"), RestoredRoot->EnterCount, 0);
	TestEqual(TEXT("Restore does not replay execute"), RestoredRoot->ExecuteCount, 0);
	TestEqual(TEXT("Pause restored"), Restored->GetStepModuleState(), EStepModuleState::Paused);
	Root->bRemoveAssetOnLeave = true;
	Module->EndStep(false);
	TestNull(TEXT("Reentrant remove applied after End traversal"), Module->GetRuntime(Asset.Get()));
	TestNull(TEXT("Current asset cleared"), Module->GetCurrentAsset());
	TestEqual(TEXT("Leave emitted once"), Root->LeaveCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStepFactoryTest, "WHFramework.Step.Graph.NativeFactory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStepFactoryTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UStepAssetFactory> Factory(NewObject<UStepAssetFactory>());
	TStrongObjectPtr<UStepAsset> First(Cast<UStepAsset>(Factory->FactoryCreateNew(UStepAsset::StaticClass(), GetTransientPackage(), NAME_None, RF_Transient, nullptr, nullptr)));
	TStrongObjectPtr<UStepAsset> Second(Cast<UStepAsset>(Factory->FactoryCreateNew(UStepAsset::StaticClass(), GetTransientPackage(), NAME_None, RF_Transient, nullptr, nullptr, NAME_None)));
	if(!TestNotNull(TEXT("Factory creates Step asset without picker"), First.Get()) || !TestNotNull(TEXT("Calling-context overload creates Step asset"), Second.Get()))
	{
		return false;
	}
	TestNotEqual(TEXT("Independent assets receive unique Start IDs"), First->StartGUID, Second->StartGUID);
	TestTrue(TEXT("Initial graph compiled"), First->bCompiled && Second->bCompiled);
	TestEqual(TEXT("Start is not a runtime step"), First->CompiledGraph.Nodes.Num(), 0);
	TestEqual(TEXT("Initial author graph is empty"), First->Steps.Num(), 0);
	TestNull(TEXT("Factory rejects unrelated class"), Factory->FactoryCreateNew(UObject::StaticClass(), GetTransientPackage(), NAME_None, RF_Transient, nullptr, nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStepRuntimeShutdownTest, "WHFramework.Step.Runtime.TerminationAndLoadReentrancy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStepRuntimeShutdownTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UStepAsset> InitAsset(NewObject<UStepAsset>());
	UStepGraphTestStep* InitFirst = StepGraphTests::Add(InitAsset.Get(), 340, 0);
	UStepGraphTestStep* InitSecond = StepGraphTests::Add(InitAsset.Get(), 340, 100);
	InitFirst->bTerminateOnInitialize = true;
	StepGraphTests::Link(InitAsset.Get(), InitAsset->StartGUID, InitFirst);
	StepGraphTests::Link(InitAsset.Get(), InitAsset->StartGUID, InitSecond);
	TStrongObjectPtr<UStepGraphTestModule> InitModule(NewObject<UStepGraphTestModule>());
	TestFalse(TEXT("Termination during initialize prevents registration"), InitModule->Prepare(InitAsset.Get()));
	TestEqual(TEXT("No later initialize callbacks after termination"), InitModule->InitializedGUIDs.Num(), 1);
	TestEqual(TEXT("Terminated module owns no runtimes"), InitModule->GetAssets().Num(), 0);

	TStrongObjectPtr<UStepAsset> Asset(NewObject<UStepAsset>());
	UStepGraphTestStep* RootTemplate = StepGraphTests::Add(Asset.Get(), 340, 0);
	UStepGraphTestStep* ChildTemplate = StepGraphTests::Add(Asset.Get(), 680, 0);
	StepGraphTests::Link(Asset.Get(), Asset->StartGUID, RootTemplate);
	StepGraphTests::Link(Asset.Get(), RootTemplate->StepGUID, ChildTemplate);
	TStrongObjectPtr<UStepGraphTestModule> Module(NewObject<UStepGraphTestModule>());
	if(!TestTrue(TEXT("Prepare termination fixture"), Module->Prepare(Asset.Get())))
	{
		return false;
	}
	Module->StartStep();
	TStrongObjectPtr<UStepGraphTestStep> Root(CastChecked<UStepGraphTestStep>(Module->GetStepByGUID(RootTemplate->StepGUID)));
	TStrongObjectPtr<UStepGraphTestStep> Child(CastChecked<UStepGraphTestStep>(Module->GetStepByGUID(ChildTemplate->StepGUID)));
	Root->bTerminateOnExecute = true;
	Module->ExecuteStep(Root.Get());
	TestTrue(TEXT("Execute callback retains traversed runtime until unwind"), Root->bRetainedDuringControlCallback);
	TestNull(TEXT("Outermost callback clears terminated runtime immediately"), Module->GetRuntime(Asset.Get()));
	TestEqual(TEXT("No new child enter after termination"), Child->EnterCount, 0);

	TStrongObjectPtr<UStepGraphTestModule> UnloadModule(NewObject<UStepGraphTestModule>());
	if(!TestTrue(TEXT("Prepare unload fixture"), UnloadModule->Prepare(Asset.Get())))
	{
		return false;
	}
	UnloadModule->StartStep();
	TStrongObjectPtr<UStepGraphTestStep> UnloadRoot(CastChecked<UStepGraphTestStep>(UnloadModule->GetStepByGUID(RootTemplate->StepGUID)));
	UnloadRoot->bUnloadOnLeave = true;
	UnloadModule->EndStep(false);
	TestTrue(TEXT("Unload does not invalidate End traversal"), UnloadRoot->bRetainedDuringControlCallback);
	TestNull(TEXT("Unload clears after End callback"), UnloadModule->GetRuntime(Asset.Get()));

	TStrongObjectPtr<UStepGraphTestModule> QueueModule(NewObject<UStepGraphTestModule>());
	if(!TestTrue(TEXT("Prepare deferred restart fixture"), QueueModule->Prepare(Asset.Get())))
	{
		return false;
	}
	QueueModule->StartStep();
	TStrongObjectPtr<UStepGraphTestStep> QueueRoot(CastChecked<UStepGraphTestStep>(QueueModule->GetStepByGUID(RootTemplate->StepGUID)));
	QueueRoot->bRestartOnLeave = true;
	QueueRoot->bTerminateOnEnter = true;
	QueueModule->EndStep(false);
	TestTrue(TEXT("Deferred Start termination retains runtime during Enter"), QueueRoot->bRetainedDuringControlCallback);
	TestNull(TEXT("Termination within command drain completes cleanup without a future tick"), QueueModule->GetRuntime(Asset.Get()));

	TStrongObjectPtr<UStepGraphTestModule> LoadModule(NewObject<UStepGraphTestModule>());
	if(!TestTrue(TEXT("Prepare nested load fixture"), LoadModule->Prepare(Asset.Get())))
	{
		return false;
	}
	LoadModule->StartStep();
	UStepGraphTestStep* LoadRoot = CastChecked<UStepGraphTestStep>(LoadModule->GetStepByGUID(RootTemplate->StepGUID));
	LoadRoot->bReloadOnExecute = true;
	LoadModule->ExecuteStep(LoadRoot);
	TestTrue(TEXT("Load Primary is rejected while executing callback"), LoadRoot->bReloadKeptRuntime);
	TestEqual(TEXT("Rejected load keeps ongoing state"), LoadRoot->StepState, EStepState::Executing);

	TStrongObjectPtr<UStepGraphTestModule> PauseModule(NewObject<UStepGraphTestModule>());
	if(!TestTrue(TEXT("Prepare callback pause fixture"), PauseModule->Prepare(Asset.Get())))
	{
		return false;
	}
	PauseModule->StartStep();
	UStepGraphTestStep* PauseRoot = CastChecked<UStepGraphTestStep>(PauseModule->GetStepByGUID(RootTemplate->StepGUID));
	UStepGraphTestStep* PauseChild = CastChecked<UStepGraphTestStep>(PauseModule->GetStepByGUID(ChildTemplate->StepGUID));
	PauseRoot->bPauseOnExecute = true;
	PauseModule->ExecuteStep(PauseRoot);
	TestEqual(TEXT("Callback pause prevents child entry"), PauseChild->EnterCount, 0);
	PauseModule->OnUnPause();
	PauseModule->OnRefresh(0.f, false);
	TestEqual(TEXT("Unpause enters child without a skipped cursor"), PauseChild->EnterCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStepGlobalTimersTest, "WHFramework.Step.Runtime.GlobalModesPreserveTimers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStepGlobalTimersTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UStepAsset> FirstAsset(NewObject<UStepAsset>());
	TStrongObjectPtr<UStepAsset> SecondAsset(NewObject<UStepAsset>());
	UStepGraphTestStep* FirstTemplate = StepGraphTests::Add(FirstAsset.Get(), 340, 0);
	UStepGraphTestStep* SecondTemplate = StepGraphTests::Add(SecondAsset.Get(), 340, 0);
	FirstTemplate->AutoExecuteStepTime = 10.f;
	SecondTemplate->AutoExecuteStepTime = 10.f;
	StepGraphTests::Link(FirstAsset.Get(), FirstAsset->StartGUID, FirstTemplate);
	StepGraphTests::Link(SecondAsset.Get(), SecondAsset->StartGUID, SecondTemplate);
	TStrongObjectPtr<UStepGraphTestModule> Module(NewObject<UStepGraphTestModule>());
	if(!TestTrue(TEXT("Register first fixture"), Module->Prepare(FirstAsset.Get())) || !TestTrue(TEXT("Register second fixture"), Module->Prepare(SecondAsset.Get())))
	{
		return false;
	}
	UStepBase* Inactive = Module->GetRuntime(FirstAsset.Get())->FindStep(FirstTemplate->StepGUID);
	Inactive->SetRuntimeState(EStepState::Entered, false);
	Inactive->RefreshAutomaticTimers();
	Module->StartStep();
	UStepBase* Active = Module->GetStepByGUID(SecondTemplate->StepGUID);
	Module->OnRefresh(3.f, false);
	Module->OnPause();
	TestEqual(TEXT("Paused remaining execution delay"), Active->CaptureRuntimeData().AutoExecuteRemaining, 7.f);
	Module->SetGlobalStepLeaveType(EStepLeaveType::Procedure);
	TestEqual(TEXT("Unrelated mode change preserves remaining delay"), Active->CaptureRuntimeData().AutoExecuteRemaining, 7.f);
	Module->SetGlobalStepExecuteType(EStepExecuteType::Procedure);
	TestEqual(TEXT("Global mode disarms current runtime"), Active->CaptureRuntimeData().AutoExecuteRemaining, -1.f);
	TestEqual(TEXT("Global mode also disarms inactive runtime"), Inactive->CaptureRuntimeData().AutoExecuteRemaining, -1.f);
	Module->SetGlobalStepExecuteType(EStepExecuteType::None);
	TestEqual(TEXT("Restoring local automatic mode arms inactive runtime"), Inactive->CaptureRuntimeData().AutoExecuteRemaining, 10.f);
	Module->OnRefresh(50.f, false);
	TestEqual(TEXT("Configuration change does not unpause timer"), Active->CaptureRuntimeData().AutoExecuteRemaining, 10.f);
	return true;
}

#endif
