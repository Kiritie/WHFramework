#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/WHFSMTestTypes.h"
#include "AI/Base/AIBlackboardBase.h"
#include "AI/Base/AIBlackboardContext.h"
#include "AI/Base/AIControllerBase.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "FSM/FSMModule.h"
#include "UObject/StrongObjectPtr.h"

namespace WHFSMTests
{
	struct FWorldFixture
	{
		TStrongObjectPtr<UWorld> World;
		AActor* Owner = nullptr;

		bool Initialize(FAutomationTestBase& Test)
		{
			World.Reset(UWorld::CreateWorld(EWorldType::Game, false));
			if (!Test.TestNotNull(TEXT("Isolated world"), World.Get()) || !Test.TestNotNull(TEXT("Engine"), GEngine))
			{
				return false;
			}
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());
			Owner = World->SpawnActor<AActor>();
			return Test.TestNotNull(TEXT("Component owner"), Owner);
		}

		UWHFSMTestComponent* MakeFSM(EFSMExecutionPolicy Policy = EFSMExecutionPolicy::Local)
		{
			UWHFSMTestComponent* FSM = NewObject<UWHFSMTestComponent>(Owner);
			FSM->DomainName = TEXT("Test.Domain");
			FSM->ExecutionPolicy = Policy;
			FSM->States = {UWHFSMTestState::StaticClass(), UWHFSMTestSecondState::StaticClass()};
			FSM->OnInitialize();
			return FSM;
		}

		~FWorldFixture()
		{
			if (World.IsValid())
			{
				World->DestroyWorld(false);
				if (GEngine)
				{
					GEngine->DestroyWorldContext(World.Get());
				}
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWHFSMTransactionTest, "WHFramework.FSM.GuardsAndOwnedStateQueries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWHFSMTransactionTest::RunTest(const FString& Parameters)
{
	WHFSMTests::FWorldFixture Fixture;
	if (!Fixture.Initialize(*this))
	{
		return false;
	}
	UWHFSMTestComponent* FSM = Fixture.MakeFSM();
	UWHFSMTestState* First = FSM->GetStateByClass<UWHFSMTestState>();
	UWHFSMTestSecondState* Second = FSM->GetStateByClass<UWHFSMTestSecondState>();
	if (!TestNotNull(TEXT("First state"), First) || !TestNotNull(TEXT("Second state"), Second))
	{
		return false;
	}
	TestTrue(TEXT("First entry succeeds"), FSM->SwitchState(First));
	Second->bPermitEnter = false;
	TestFalse(TEXT("Rejected entry returns failure"), FSM->SwitchState(Second));
	TestEqual(TEXT("Rejected transition keeps the current state"), FSM->GetCurrentState(), static_cast<UFiniteStateBase*>(First));
	TestEqual(TEXT("Rejected transition never leaves the old state"), First->LeaveCount, 0);
	TestEqual(TEXT("Rejected transition never starts new gameplay"), Second->EnterCount, 0);
	Second->bPermitEnter = true;
	First->bPermitLeave = false;
	TestFalse(TEXT("Leave guard vetoes transition"), FSM->SwitchState(Second));
	First->bPermitLeave = true;
	TestTrue(TEXT("Accepted transition succeeds"), FSM->SwitchState(Second));
	TestEqual(TEXT("Old state leaves exactly once"), First->LeaveCount, 1);
	TestEqual(TEXT("New state enters exactly once"), Second->EnterCount, 1);

	TArray<UFiniteStateBase*> QueryA = FSM->GetStates();
	UWHFSMTestComponent* Other = Fixture.MakeFSM();
	const TArray<UFiniteStateBase*> QueryB = Other->GetStates();
	TestTrue(TEXT("Different components own different state objects"), QueryA[0] != QueryB[0]);
	QueryA.Reset();
	TestEqual(TEXT("Changing a query copy cannot mutate the FSM"), FSM->GetStates().Num(), 2);
	TestEqual(TEXT("Another query does not overwrite previous query storage"), QueryB.Num(), 2);
	FSM->OnTermination();
	Other->OnTermination();

	UWHFSMTestComponent* DomainFSM = NewObject<UWHFSMTestComponent>(Fixture.Owner);
	DomainFSM->DomainName = TEXT("Test.Domain");
	DomainFSM->ExecutionPolicy = EFSMExecutionPolicy::Local;
	DomainFSM->States = {UWHFSMTestState::StaticClass(), UWHFSMTestOtherDomainState::StaticClass()};
	AddExpectedError(TEXT("FSM状态配置无效"), EAutomationExpectedErrorFlags::Contains, 1);
	DomainFSM->OnInitialize();
	TestFalse(TEXT("A state from a different domain is not registered"), DomainFSM->HasStateByClass<UWHFSMTestOtherDomainState>());
	DomainFSM->OnTermination();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWHFSMTerminationTest, "WHFramework.FSM.TerminationAndReentrancy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWHFSMTerminationTest::RunTest(const FString& Parameters)
{
	WHFSMTests::FWorldFixture Fixture;
	if (!Fixture.Initialize(*this))
	{
		return false;
	}
	UWHFSMTestComponent* FSM = Fixture.MakeFSM();
	UWHFSMTestState* First = FSM->GetStateByClass<UWHFSMTestState>();
	First->bRequestNestedSwitch = true;
	TestTrue(TEXT("Outer transition is accepted"), FSM->SwitchState(First));
	TestFalse(TEXT("Nested transition is explicitly rejected"), First->bNestedSwitchAccepted);
	First->bPermitLeave = false;
	TestTrue(TEXT("Termination cannot be blocked by a gameplay leave guard"), FSM->TerminateState(First));
	TestEqual(TEXT("Active state leaves once before removal"), First->LeaveCount, 1);
	TestEqual(TEXT("Removed state terminates once"), First->TerminationCount, 1);
	TestFalse(TEXT("Double termination is ignored"), FSM->TerminateState(First));
	FSM->OnTermination();
	FSM->OnTermination();
	TestEqual(TEXT("Component teardown does not terminate removed state twice"), First->TerminationCount, 1);

	UWHFSMTestComponent* DeferredFSM = Fixture.MakeFSM();
	UWHFSMTestState* Deferred = DeferredFSM->GetStateByClass<UWHFSMTestState>();
	Deferred->bTerminateDuringEnter = true;
	DeferredFSM->SwitchState(Deferred);
	TestEqual(TEXT("Deferred teardown enters once"), Deferred->EnterCount, 1);
	TestEqual(TEXT("Deferred teardown leaves once"), Deferred->LeaveCount, 1);
	TestEqual(TEXT("Deferred teardown terminates once"), Deferred->TerminationCount, 1);
	TestNull(TEXT("Deferred teardown clears current state"), DeferredFSM->GetCurrentState());
	DeferredFSM->OnTermination();
	TestEqual(TEXT("Repeated teardown remains idempotent"), Deferred->LeaveCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWHFSMPresentationTest, "WHFramework.FSM.ReplicatedPresentationHasNoGameplay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWHFSMPresentationTest::RunTest(const FString& Parameters)
{
	WHFSMTests::FWorldFixture Fixture;
	if (!Fixture.Initialize(*this))
	{
		return false;
	}
	UWHFSMTestComponent* FSM = Fixture.MakeFSM(EFSMExecutionPolicy::None);
	UWHFSMTestState* First = FSM->GetStateByClass<UWHFSMTestState>();
	UWHFSMTestSecondState* Second = FSM->GetStateByClass<UWHFSMTestSecondState>();
	FFSMReplicatedState Snapshot;
	Snapshot.Domain = TEXT("Test.Domain");
	Snapshot.State = TEXT("Test.First");
	Snapshot.Revision = 1;
	Snapshot.StartedAt = 10.0;
	FSM->ReceiveSnapshot(Snapshot);
	TestEqual(TEXT("Snapshot never invokes gameplay enter"), First->EnterCount, 0);
	TestEqual(TEXT("Late join applies current presentation"), First->PresentationEnterCount, 1);
	FSM->ReceiveSnapshot(Snapshot);
	TestEqual(TEXT("Duplicate revision is idempotent"), First->PresentationEnterCount, 1);
	Snapshot.Revision = 3;
	FSM->ReceiveSnapshot(Snapshot);
	TestEqual(TEXT("A new activation of the same state is presented"), First->PresentationEnterCount, 2);
	TestEqual(TEXT("Previous presentation is closed before replay"), First->PresentationLeaveCount, 1);
	Snapshot.State = TEXT("Test.Second");
	Snapshot.Revision = 4;
	FSM->ReceiveSnapshot(Snapshot);
	TestEqual(TEXT("Next snapshot never invokes gameplay leave"), First->LeaveCount, 0);
	TestEqual(TEXT("Next snapshot applies new presentation"), Second->PresentationEnterCount, 1);
	TestFalse(TEXT("A presentation-only FSM cannot run gameplay transitions"), FSM->SwitchState(First));
	FSM->OnTermination();
	TestEqual(TEXT("Client teardown never invokes gameplay leave"), Second->LeaveCount, 0);
	TestEqual(TEXT("Client teardown releases presentation once"), Second->PresentationLeaveCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWHFSMGroupTest, "WHFramework.FSM.GroupRegistrationAndRename", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWHFSMGroupTest::RunTest(const FString& Parameters)
{
	WHFSMTests::FWorldFixture Fixture;
	if (!Fixture.Initialize(*this))
	{
		return false;
	}
	UFSMModule* Module = NewObject<UFSMModule>(Fixture.Owner);
	UWHFSMTestComponent* FSM = NewObject<UWHFSMTestComponent>(Fixture.Owner);
	FSM->GroupName = TEXT("First");
	Module->RegisterFSM(FSM);
	TestEqual(TEXT("First registration inserts a member"), Module->GetGroup(TEXT("First")).FSMArray.Num(), 1);
	Module->RegisterFSM(FSM);
	TestEqual(TEXT("Repeated registration has no duplicates"), Module->GetGroup(TEXT("First")).FSMArray.Num(), 1);
	FSM->GroupName = TEXT("Second");
	Module->RegisterFSM(FSM);
	TestFalse(TEXT("Re-registering a renamed component removes its old group"), Module->HasGroup(TEXT("First")));
	TestEqual(TEXT("Renamed component belongs to its new group"), Module->GetGroup(TEXT("Second")).FSMArray.Num(), 1);
	FSM->GroupName = TEXT("UnregisteredName");
	Module->UnregisterFSM(FSM);
	TestFalse(TEXT("Unregister uses object identity even after a name edit"), Module->HasGroup(TEXT("Second")));
	Module->UnregisterFSM(FSM);
	TestEqual(TEXT("Missing group lookup returns an independent empty value"), Module->GetGroup(TEXT("Missing")).FSMArray.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWHBlackboardOwnershipTest, "WHFramework.AI.BlackboardIsolationAndRepossess", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWHBlackboardOwnershipTest::RunTest(const FString& Parameters)
{
	WHFSMTests::FWorldFixture Fixture;
	if (!Fixture.Initialize(*this))
	{
		return false;
	}
	UBehaviorTree* Tree = NewObject<UBehaviorTree>(Fixture.World.Get());
	UAIBlackboardBase* Definition = NewObject<UAIBlackboardBase>(Tree);
	Tree->BlackboardAsset = Definition;
	AAIControllerBase* ControllerA = Fixture.World->SpawnActor<AAIControllerBase>();
	AAIControllerBase* ControllerB = Fixture.World->SpawnActor<AAIControllerBase>();
	AWHBlackboardTestPawn* PawnA = Fixture.World->SpawnActor<AWHBlackboardTestPawn>();
	AWHBlackboardTestPawn* PawnB = Fixture.World->SpawnActor<AWHBlackboardTestPawn>();
	AWHBlackboardTestPawn* PawnC = Fixture.World->SpawnActor<AWHBlackboardTestPawn>();
	ControllerA->Possess(PawnA);
	ControllerB->Possess(PawnB);
	if (!TestTrue(TEXT("First controller creates its runtime"), ControllerA->SetBehaviorTree(Tree, false)) ||
		!TestTrue(TEXT("Second controller creates its runtime"), ControllerB->SetBehaviorTree(Tree, false)))
	{
		return false;
	}
	UAIBlackboardContext* OldContext = ControllerA->GetBlackboard();
	TestTrue(TEXT("Shared definition creates distinct mutable runtimes"), OldContext != ControllerB->GetBlackboard());
	OldContext->SetTargetAgent(PawnB);
	OldContext->SetIsLostTarget(true);
	TestNull(TEXT("First controller's target is not written into the second"), ControllerB->GetBlackboard()->GetTargetAgent());
	TestFalse(TEXT("Runtime flags remain per controller"), ControllerB->GetBlackboard()->GetIsLostTarget());
	TestEqual(TEXT("Blackboard writes do not replace the selected tree"), ControllerA->GetBehaviorTree(), Tree);

	ControllerA->UnPossess();
	ControllerA->Possess(PawnC);
	TestTrue(TEXT("Same tree can be rebound to a new pawn"), ControllerA->SetBehaviorTree(Tree, false));
	TestTrue(TEXT("Repossess creates a new runtime"), OldContext != ControllerA->GetBlackboard());
	TestNull(TEXT("Released runtime has no stale pawn"), OldContext->GetAgent<APawn>());
	TestEqual(TEXT("New runtime points to the new pawn"), ControllerA->GetBlackboard()->GetAgent<APawn>(), static_cast<APawn*>(PawnC));
	TestNull(TEXT("New possession starts with no previous target"), ControllerA->GetBlackboard()->GetTargetAgent());
	TestFalse(TEXT("New possession starts with reset flags"), ControllerA->GetBlackboard()->GetIsLostTarget());
	ControllerA->UnPossess();
	ControllerB->UnPossess();
	return true;
}

#endif
