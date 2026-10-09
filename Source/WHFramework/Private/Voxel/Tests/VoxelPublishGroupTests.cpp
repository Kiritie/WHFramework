#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Voxel/Rendering/VoxelPublishGroups.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/VoxelModule.h"


namespace
{
	void TickPublisherFrame(FVoxelViewPublisher& InPublisher, UVoxelModule& InModule)
	{
		auto& Frame = InModule.GetTerrainFrameContext();
		const bool bOwnFrame = !Frame.IsActive();
		if (bOwnFrame) Frame.Begin(1, Frame.GetFrameCounter() + 1, FPlatformTime::Seconds(), {});
		InPublisher.Tick(Frame.GetPublishBudget());
		if (bOwnFrame) Frame.End(FPlatformTime::Seconds());
	}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPublishGroupContinuationTest,
	"WHFramework.Voxel.Rendering.PublishGroupContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPublishGroupContinuationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TArray<FVoxelPublishFootprint> Footprints;
	for (int32 Index = 0; Index < 256; ++Index)
	{
		const double X = Index * 64.0;
		Footprints.Add({{0, FIntVector(Index * 4, 0, 0), 0}, FBox(FVector(X, 0, 0), FVector(X + 16, 16, 16)), false});
	}
	Footprints.Add({{1, FIntVector::ZeroValue, 1}, FBox(FVector(0, 0, 0), FVector(32, 32, 32)), false});
	Footprints.Add({{0, FIntVector(0, 0, 8), 0}, FBox(FVector(0, 0, 128), FVector(16, 16, 144)), false});
	Footprints.Add({{2, FIntVector(100, 100, 0), 1}, FBox(ForceInit), false});
	Footprints.Add({{4, FIntVector(100, 100, 0), 1}, FBox(ForceInit), true});
	const FVoxelPublishFootprint Duplicate = Footprints[0];
	Footprints.Add(Duplicate);
	TArray<int32> Expected;
	FVoxelPublishGroupPlanner::Build(Footprints, Expected);
	FVoxelPublishGroupContinuation Continuation;
	for (const auto& Footprint : Footprints) TestTrue(TEXT("A local footprint fits planning capacity"), Continuation.Add(Footprint));
	int32 Steps = 0;
	while (!Continuation.Advance(1) && ++Steps < 20000) {}
	TestTrue(TEXT("Bounded planning completes"), Steps < 20000);
	TestTrue(TEXT("Planning continues across many independent operations"), Steps > Footprints.Num());
	const auto& Actual = Continuation.GetGroupIndices();
	for (int32 First = 0; First < Actual.Num(); ++First)
		for (int32 Second = 0; Second < Actual.Num(); ++Second)
			if ((Actual[First] == Actual[Second]) != (Expected[First] == Expected[Second]))
				AddError(FString::Printf(TEXT("Component mismatch at %d/%d"), First, Second));
	TestTrue(TEXT("Interval pruning avoids all-pairs comparisons for distant owners"), Continuation.GetComparedFootprints() < 2000);
	TestFalse(TEXT("The frozen graph rejects additional input"), Continuation.Add(Footprints[0]));
	FVoxelPublishBoundsIndex Index;
	for (int32 Ordinal = 0; Ordinal < 2048; ++Ordinal)
		Index.Add(Ordinal, FBox(FVector(0, Ordinal * 1600.0, 0), FVector(16, Ordinal * 1600.0 + 16, 16)));
	FVoxelPublishBoundsIndex::FQuery Query;
	Index.BeginQuery(FBox(FVector(-1, -1, -1), FVector(17, 17, 17)), Query);
	int32 Operations = 0;
	TSet<int32> Matches;
	while (++Operations < 2048)
	{
		int32 Candidate = INDEX_NONE;
		if (Index.AdvanceQuery(Query, Candidate)) break;
		if (Candidate != INDEX_NONE) Matches.Add(Candidate);
	}
	TestEqual(TEXT("A local query excludes distant owners sharing its X interval"), Matches.Num(), 1);
	TestTrue(TEXT("A local query resolves its exact owner"), Matches.Contains(0));
	TestTrue(TEXT("Query work does not scan the 2048 distant owners"), Operations < 512);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFrozenGenerationProgressTest,
	"WHFramework.Voxel.Rendering.FrozenGenerationProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFrozenGenerationProgressTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler(true);
	if (!TestTrue(TEXT("发布夹具初始化真实控制身份与统计账本"), Scheduler.InitializeStatistics(1))) return false;
	FVoxelViewPublisher Publisher(*Module, Scheduler, 1);
	const FVoxelPublishGroupKey Owner{0, FIntVector::ZeroValue, 1};
	int32 Commits = 0;
	Publisher.SetCommitCallbacks({}, [&Commits](TConstArrayView<FVoxelPublishGroupKey>) { ++Commits; });
	const FGuid Frozen = Publisher.BeginGroup(Owner);
	TestTrue(TEXT("The first generation is committed before planning freezes"), Publisher.CommitGroup(Frozen));
	Publisher.AdvancePendingPlan();
	const FGuid Newer = Publisher.BeginGroup(Owner);
	TestNotEqual(TEXT("An immutable committed generation gets a separate successor"), Frozen, Newer);
	TestTrue(TEXT("The successor commit is accepted"), Publisher.CommitGroup(Newer));
	int32 Steps = 0;
	while (Publisher.PendingPlan && ++Steps < 256) Publisher.AdvancePendingPlan();
	TestTrue(TEXT("Frozen planning completes within bounded work"), Steps < 256);
	TestEqual(TEXT("New input cannot starve an already committed generation"), Publisher.Publications.Num(), 1);
	TestTrue(TEXT("The successor remains queued with its own handle"), Publisher.PendingGroupIds.FindRef(Owner) == Newer);
	for (int32 Tick = 0; Tick < 8 && Publisher.IsBusy(); ++Tick) TickPublisherFrame(Publisher, *Module);
	TestEqual(TEXT("Both immutable generations commit once in owner order"), Commits, 2);
	TestFalse(TEXT("Both generations release their owner"), Publisher.IsGroupBusy(Owner));
	Publisher.Reset();
	for (const bool bSharedOwner : {true, false})
	{
		TArray<uint64> CommitOrder;
		TArray<FVoxelViewPublisher::FPublicationRef> Generations;
		for (int32 Generation = 0; Generation < 2; ++Generation)
		{
			const auto Publication = MakeShared<FVoxelViewPublisher::FPublication>();
			Publication->Serial = ++Publisher.BatchSerial;
			Publication->bCoveragePrepared = true;
			Publication->bBuildOrderInitialized = true;
			const FVoxelPublishGroupKey Key = Generation == 0 || bSharedOwner
				? Owner : FVoxelPublishGroupKey{1, FIntVector::ZeroValue, 1};
			Publication->Callbacks.Add(Key, MakeShared<FVoxelViewPublisher::FGroupCallback>(
				[&CommitOrder, Serial = Publication->Serial](TConstArrayView<FVoxelPublishGroupKey>) { CommitOrder.Add(Serial); }));
			auto& Group = Publication->Groups.AddDefaulted_GetRef();
			Group.Keys.Add(Key);
			Group.Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Key, Module->BlockSize());
			Publisher.Publications.Add(Publication);
			Publisher.RefreshGroupOrder(Publication);
			Publisher.SetOwnerActive(Key, Publication->Serial, true);
			Publisher.QueueReadyGroup(Publication, 0);
			Group.FirstQueuedAt = FPlatformTime::Seconds() - (Generation == 1 ? 2.0 : 0.0);
			Generations.Add(Publication);
		}
		Publisher.bPrioritiesDirty = true;
		for (int32 Tick = 0; Tick < 16 && Publisher.IsBusy(); ++Tick) TickPublisherFrame(Publisher, *Module);
		TestTrue(TEXT("An overdue successor cannot monopolize service while waiting for its older dependency"),
			CommitOrder == TArray<uint64>{Generations[0]->Serial, Generations[1]->Serial});
		TestFalse(TEXT("Owner and spatial dependencies both release after ordered commits"), Publisher.IsBusy());
		Publisher.Reset();
	}
	const FVoxelPublishGroupKey Independent{0, FIntVector(1000, 0, 0), 1};
	const auto ReadyPublication = MakeShared<FVoxelViewPublisher::FPublication>();
	ReadyPublication->Serial = ++Publisher.BatchSerial;
	ReadyPublication->bCoveragePrepared = true;
	ReadyPublication->bBuildOrderInitialized = true;
	auto& ReadyGroup = ReadyPublication->Groups.AddDefaulted_GetRef();
	ReadyGroup.Keys.Add(Independent);
	ReadyGroup.bPriorityReady = true;
	Publisher.Publications.Add(ReadyPublication);
	Publisher.RefreshGroupOrder(ReadyPublication);
	Publisher.SetOwnerActive(Independent, ReadyPublication->Serial, true);
	Publisher.QueueReadyGroup(ReadyPublication, 0);
	Publisher.PriorityPlan = MakeUnique<FVoxelViewPublisher::FPriorityPlan>();
	Publisher.bPrioritiesDirty = true;
	Publisher.NextPhase = 4;
	FVoxelPublishBudget CommitSlice;
	CommitSlice.Begin(FPlatformTime::Seconds(), 100.0, 1);
	Publisher.Tick(CommitSlice);
	TestTrue(TEXT("Ready local work commits while unrelated priority planning remains unfinished"), ReadyGroup.bCommitted);
	Publisher.Reset();
	Publisher.BeginGroup(Owner);
	Publisher.CommitGroup(Publisher.BeginGroup(Independent));
	int32 Frames = 0;
	while (Publisher.IsGroupBusy(Independent) && ++Frames < 512)
	{
		const TArray<FVector> MovingObservers{FVector(Frames, 0, 0)};
		Publisher.SetObservers(MovingObservers);
		FVoxelPublishBudget Budget;
		Budget.Begin(FPlatformTime::Seconds(), 100.0, 1);
		Publisher.Tick(Budget);
	}
	TestTrue(TEXT("Moving observers preserve publication progress under a single-operation frame"), Frames > 1 && Frames < 512);
	TestFalse(TEXT("Independent publication finishes while the other owner remains open"), Publisher.IsGroupBusy(Independent));
	TestTrue(TEXT("The unrelated open owner remains outstanding"), Publisher.IsGroupBusy(Owner));
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelIndependentPublishCommitTest,
	"WHFramework.Voxel.Rendering.IndependentGroupCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelIndependentPublishCommitTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler(true);
	if (!TestTrue(TEXT("发布夹具初始化真实控制身份与统计账本"), Scheduler.InitializeStatistics(1))) return false;
	FVoxelViewPublisher Publisher(*Module, Scheduler, 1);
	const FVoxelPublishGroupKey Parent{1, {0, 0}, 1};
	const FVoxelPublishGroupKey Child{0, {0, 0}, 0};
	const FVoxelPublishGroupKey Distant{0, {100, 100}, 0};
	const FGuid ParentHandle = Publisher.BeginGroup(Parent);
	const FGuid ChildHandle = Publisher.BeginGroup(Child);
	const FGuid DistantHandle = Publisher.BeginGroup(Distant);
	TSet<FVoxelPublishGroupKey> Committed;
	int32 Completed = 0;
	Publisher.SetCommitCallbacks([&Completed]() { ++Completed; },
		[&Committed](TConstArrayView<FVoxelPublishGroupKey> Keys)
		{
			for (const FVoxelPublishGroupKey& Key : Keys) Committed.Add(Key);
		});
	TestTrue(TEXT("Parent commit is accepted"), Publisher.CommitGroup(ParentHandle));
	TestTrue(TEXT("Distant commit is accepted"), Publisher.CommitGroup(DistantHandle));
	TickPublisherFrame(Publisher, *Module);
	TestTrue(TEXT("Distant group commits while a connected child is uncommitted"), Committed.Contains(Distant));
	TestFalse(TEXT("Parent waits for its connected child"), Committed.Contains(Parent));
	TestTrue(TEXT("Connected parent remains busy"), Publisher.IsGroupBusy(Parent));
	TestFalse(TEXT("Committed distant group releases its handle"), Publisher.IsGroupBusy(Distant));
	TestEqual(TEXT("Completion waits for all requested groups"), Completed, 0);
	TestFalse(TEXT("A consumed handle cannot commit twice"), Publisher.CommitGroup(DistantHandle));
	TestTrue(TEXT("Child commit is accepted after independent publication"), Publisher.CommitGroup(ChildHandle));
	for (int32 Index = 0; Index < 4 && Publisher.IsBusy(); ++Index) TickPublisherFrame(Publisher, *Module);
	TestTrue(TEXT("Parent eventually commits"), Committed.Contains(Parent));
	TestTrue(TEXT("Child eventually commits"), Committed.Contains(Child));
	TestEqual(TEXT("Completion runs once after the final component"), Completed, 1);
	TestFalse(TEXT("All committed groups finish"), Publisher.IsBusy());
	const FGuid Reused = Publisher.BeginGroup(Distant);
	TestTrue(TEXT("A released key receives a fresh handle"), Reused.IsValid() && Reused != DistantHandle);
	Publisher.Reset();
	TestFalse(TEXT("Reset invalidates pending handles"), Publisher.CommitGroup(Reused));
	TestFalse(TEXT("Reset leaves no publication in progress"), Publisher.IsBusy());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelActiveGroupReplacementTest,
	"WHFramework.Voxel.Rendering.ActiveGroupReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelConcurrentPublishCommitTest,
	"WHFramework.Voxel.Rendering.ConcurrentIndependentGroupCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelConcurrentPublishCommitTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler(true);
	if (!TestTrue(TEXT("发布夹具初始化真实控制身份与统计账本"), Scheduler.InitializeStatistics(1))) return false;
	FVoxelViewPublisher Publisher(*Module, Scheduler, 1);
	const auto SignalFixtureReady = [&Publisher](const FVoxelViewPublisher::FPublicationRef& Ready)
	{
		for (int32 GroupIndex = 0; GroupIndex < Ready->Groups.Num(); ++GroupIndex)
			for (const int32 UpdateIndex : Ready->Groups[GroupIndex].UpdateIndices)
				Ready->Updates[UpdateIndex].GroupIndex = GroupIndex;
		Publisher.RefreshGroupOrder(Ready);
		for (int32 Index = 0; Index < Ready->Updates.Num(); ++Index) Publisher.IndexPublicationUpdate(Ready, Index);
		for (const auto& Pair : Ready->Visibility) Publisher.ActorVisibilityGenerations.FindOrAdd(Pair.Key).Add({Ready, Pair.Value});
		for (const auto& Group : Ready->Groups)
			for (const auto& Key : Group.Keys) Publisher.SetOwnerActive(Key, Ready->Serial, true);
		Publisher.bPrioritiesDirty = true;
		for (int32 Index = 0; Index < Ready->Groups.Num(); ++Index) Publisher.QueueReadyGroup(Ready, Index);
		Publisher.bFirstOwnersDirty = true;
		Publisher.bActiveBusyKeysDirty = true;
	};
	const FVoxelPublishGroupKey Slow{0, {0, 0}, 0}, Distant{0, {1000, 0}, 0};
	int32 OldCallbacks = 0, NewCallbacks = 0;
	const auto Publication = MakeShared<FVoxelViewPublisher::FPublication>();
	Publication->Serial = ++Publisher.BatchSerial;
	Publication->bCoveragePrepared = true;
	Publication->Updates.SetNum(1);
	Publication->PreparedMeshes.SetNum(1);
	auto& Group = Publication->Groups.AddDefaulted_GetRef();
	Group.Keys.Add(Slow);
	Group.UpdateIndices.Add(0);
	Publication->Callbacks.Add(Slow, MakeShared<FVoxelViewPublisher::FGroupCallback>(
		[&OldCallbacks](TConstArrayView<FVoxelPublishGroupKey>) { ++OldCallbacks; }));
	Publisher.Publications.Add(Publication);
	SignalFixtureReady(Publication);
	TestTrue(TEXT("The active owner lookup is populated before a new pending generation"), Publisher.IsGroupBusy(Slow));
	const FGuid DistantHandle = Publisher.BeginGroup(Distant);
	// 提交前赋予新组回调；原活跃组持有自己的不可变回调。
	Publisher.SetCommitCallbacks({}, [&NewCallbacks, Distant](TConstArrayView<FVoxelPublishGroupKey> Keys)
	{
		for (const auto& Key : Keys) if (Key == Distant) ++NewCallbacks;
	});
	Publisher.CommitGroup(DistantHandle);
	Publication->Updates[0].bUnchanged = true;
	TickPublisherFrame(Publisher, *Module);
	TestTrue(TEXT("A slow active publication remains in flight"), Publisher.IsGroupBusy(Slow));
	TestFalse(TEXT("A newly committed independent owner finishes while an older mesh is missing"), Publisher.IsGroupBusy(Distant));
	TestEqual(TEXT("Only the independent request callback runs"), NewCallbacks, 1);
	TestEqual(TEXT("The waiting old callback remains untouched"), OldCallbacks, 0);
	Publication->PreparedMeshes[0] = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	SignalFixtureReady(Publication);
	TickPublisherFrame(Publisher, *Module);
	TestEqual(TEXT("The old publication retains its own callback"), OldCallbacks, 1);
	TestEqual(TEXT("The new callback is not reused for an old group"), NewCallbacks, 1);
	TestFalse(TEXT("Both independent publications finish"), Publisher.IsBusy());
	TestFalse(TEXT("Committing the final owner invalidates its cached busy lookup"), Publisher.IsGroupBusy(Slow));
	const auto LogicalPublication = MakeShared<FVoxelViewPublisher::FPublication>();
	LogicalPublication->Callbacks.Add({1, {0, 0}, 1}, {});
	LogicalPublication->Callbacks.Add({0, {0, 0}, 0}, {});
	Publisher.BuildGroups(LogicalPublication);
	TestEqual(TEXT("Empty child ownership joins its parent handoff"), LogicalPublication->Groups.Num(), 1);
	if (LogicalPublication->Groups.Num() == 1)
		TestEqual(TEXT("Both logical owner keys commit in one component"), LogicalPublication->Groups[0].Keys.Num(), 2);
	UWorld* FixtureWorld = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(FixtureWorld);
	AActor* Actor = FixtureWorld->SpawnActor<AActor>();
	const FVoxelPublishGroupKey Surface{2, {0, 0}, 0};
	auto& Entry = Publisher.Entries.Add(Actor);
	Entry.GroupKey = Surface;
	Entry.Scale = Module->BlockSize();
	Entry.Bounds = FBox(FVector::ZeroVector, FVector(32.0));
	Entry.PresentedWorldBounds = FBox(FVector::ZeroVector, FVector(32.0) * Entry.Scale);
	const auto CommittedSource = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	Entry.Source = Entry.PresentedSource = CommittedSource;
	Entry.bPresented = true;
	Entry.bDirty = false;
	Entry.LocalWorldBoxes = {FBox(FVector(0, 0, -100), FVector(4, 4, 100))};
	Entry.Source = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	Entry.bDirty = true;
	const auto Shared = MakeShared<TArray<FBox>, ESPMode::ThreadSafe>();
	Shared->Add(FBox(FVector(8, 8, -100), FVector(12, 12, 100)));
	const FGuid SharedGroup = Publisher.BeginGroup(Surface);
	TestTrue(TEXT("Fine can clip a committed coarse owner while its replacement topology is pending"),
		Publisher.SetSharedCoverage(SharedGroup, Actor, Shared));
	TestTrue(TEXT("Shared clipping preserves the committed source instead of publishing the pending replacement"),
		Publisher.PendingUpdates.Num() == 1 && Publisher.PendingUpdates[0].Source == CommittedSource);
	Publisher.CommitGroup(SharedGroup);
	const double CoverageDeadline = FPlatformTime::Seconds() + 10.0;
	while (Publisher.IsBusy() && FPlatformTime::Seconds() < CoverageDeadline)
	{
		TickPublisherFrame(Publisher, *Module);
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestFalse(TEXT("A retained coarse owner completes its Fine clipping publication"), Publisher.IsBusy());
	TestEqual(TEXT("Shared clipping preserves the committed coarse layer exclusion"), Entry.LocalWorldBoxes.Num(), 1);
	TestEqual(TEXT("Both coarse and Fine exclusions reach the committed coverage"), Entry.Exclusions.Num(), 2);
	TestTrue(TEXT("The pending coarse source remains dirty after retained-source clipping"), Entry.bDirty);
	TestTrue(TEXT("Retained-source clipping keeps the previous committed source"), Entry.PresentedSource == CommittedSource);
	const FGuid RetryGroup = Publisher.BeginGroup(Surface);
	const auto ChangedShared = MakeShared<TArray<FBox>, ESPMode::ThreadSafe>();
	ChangedShared->Add(FBox(FVector(16, 16, -100), FVector(20, 20, 100)));
	Publisher.SetSharedCoverage(RetryGroup, Actor, ChangedShared);
	Publisher.CommitGroup(RetryGroup);
	TickPublisherFrame(Publisher, *Module);
	if (TestEqual(TEXT("Cancellation fixture has one immutable publication"), Publisher.Publications.Num(), 1))
	{
		const auto RetriedPublication = Publisher.Publications[0];
		Scheduler.CancelMatching([](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::BuildViewCoverage && Stamp.Section.Y == 2;
		});
		const double RetryDeadline = FPlatformTime::Seconds() + 10.0;
		while (RetriedPublication->bCoveragePreparing && FPlatformTime::Seconds() < RetryDeadline)
		{
			Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
			FPlatformProcess::Sleep(0.001f);
		}
		TestFalse(TEXT("Canceled coverage preparation preserves the publication"), RetriedPublication->bCanceled);
		TestTrue(TEXT("Cancellation preserves the old presented source"), Entry.PresentedSource == CommittedSource);
		while (RetriedPublication->PendingBuildIndices.IsEmpty() && Publisher.IsBusy() &&
			FPlatformTime::Seconds() < RetryDeadline)
		{
			TickPublisherFrame(Publisher, *Module);
			if (!RetriedPublication->PendingBuildIndices.IsEmpty()) break;
			Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
			FPlatformProcess::Sleep(0.001f);
		}
		TestFalse(TEXT("Retry advances to a real clipping task"), RetriedPublication->PendingBuildIndices.IsEmpty());
		Scheduler.CancelMatching([](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
		{
			return Kind == EVoxelTaskKind::BuildViewCoverage && Stamp.Section.Y == 3;
		});
		while (Publisher.IsBusy() && FPlatformTime::Seconds() < RetryDeadline)
		{
			Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
			TickPublisherFrame(Publisher, *Module);
			FPlatformProcess::Sleep(0.001f);
		}
		TestFalse(TEXT("Canceled clipping retries the same publication without discarding it"), RetriedPublication->bCanceled);
		TestFalse(TEXT("Both canceled dependency tasks eventually commit"), Publisher.IsBusy());
		TestEqual(TEXT("Canceled dependency retries leave no duplicate builds"), RetriedPublication->PendingBuildIndices.Num(), 0);
	}
	const auto WaitingFine = MakeShared<FVoxelViewPublisher::FPublication>();
	WaitingFine->Serial = ++Publisher.BatchSerial;
	WaitingFine->bCoveragePrepared = true;
	WaitingFine->Updates.SetNum(1);
	WaitingFine->Updates[0].bUnchanged = true;
	WaitingFine->PreparedMeshes.SetNum(1);
	auto& WaitingGroup = WaitingFine->Groups.AddDefaulted_GetRef();
	WaitingGroup.Keys.Add(Slow);
	WaitingGroup.Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Slow, Module->BlockSize());
	WaitingGroup.UpdateIndices.Add(0);
	Publisher.Publications.Add(WaitingFine);
	SignalFixtureReady(WaitingFine);
	const FGuid NoopGroup = Publisher.BeginGroup(Surface);
	Publisher.SetSharedCoverage(NoopGroup, Actor, ChangedShared);
	Publisher.SetOwnershipChanged(NoopGroup, false);
	Publisher.CommitGroup(NoopGroup);
	const double NoopDeadline = FPlatformTime::Seconds() + 10.0;
	while (Publisher.IsGroupBusy(Surface) && FPlatformTime::Seconds() < NoopDeadline)
	{
		TickPublisherFrame(Publisher, *Module);
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestFalse(TEXT("An unchanged overlapping coarse owner finishes independently"), Publisher.IsGroupBusy(Surface));
	TestTrue(TEXT("Unchanged coarse comparison does not wait for an unrelated unfinished Fine mesh"),
		Publisher.IsGroupBusy(Slow));
	const FGuid ChangedGroup = Publisher.BeginGroup(Surface);
	const auto NewShared = MakeShared<TArray<FBox>, ESPMode::ThreadSafe>();
	NewShared->Add(FBox(FVector(10, 10, -100), FVector(14, 14, 100)));
	Publisher.SetSharedCoverage(ChangedGroup, Actor, NewShared);
	Publisher.SetOwnershipChanged(ChangedGroup, false);
	Publisher.CommitGroup(ChangedGroup);
	for (int32 Tick = 0; Tick < 16; ++Tick)
	{
		TickPublisherFrame(Publisher, *Module);
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("A real overlapping coverage change retains its older dependency"), Publisher.IsGroupBusy(Surface));
	const auto Overlapping = Publisher.Publications.FindByPredicate([&](const auto& Candidate)
	{
		return Candidate->Serial > WaitingFine->Serial && Candidate->Callbacks.Contains(Surface);
	});
	TestTrue(TEXT("The clipping fixture produces a changed region inside its delayed Fine owner"),
		Overlapping && (*Overlapping)->Updates.ContainsByPredicate([&WaitingGroup](const auto& Update)
		{
			return Update.ChangedBounds.ContainsByPredicate([&WaitingGroup](const FBox& Bounds)
			{
				return FMath::Min(Bounds.Max.X, WaitingGroup.Bounds.Max.X) > FMath::Max(Bounds.Min.X, WaitingGroup.Bounds.Min.X) &&
					FMath::Min(Bounds.Max.Y, WaitingGroup.Bounds.Max.Y) > FMath::Max(Bounds.Min.Y, WaitingGroup.Bounds.Min.Y);
			});
		}));
	WaitingFine->PreparedMeshes[0] = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	SignalFixtureReady(WaitingFine);
	while (Publisher.IsBusy() && FPlatformTime::Seconds() < NoopDeadline)
	{
		TickPublisherFrame(Publisher, *Module);
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestFalse(TEXT("A real coverage change commits after its older overlapping Fine dependency"), Publisher.IsBusy());
	const auto ProjectedOwner = MakeShared<FVoxelViewPublisher::FPublication>();
	ProjectedOwner->Serial = ++Publisher.BatchSerial;
	ProjectedOwner->bCoveragePrepared = true;
	ProjectedOwner->Updates.SetNum(1);
	ProjectedOwner->PreparedMeshes.SetNum(1);
	auto& ProjectedUpdate = ProjectedOwner->Updates[0];
	ProjectedUpdate.Actor = Actor;
	ProjectedUpdate.GroupIndex = 0;
	const auto ProjectedSource = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	ProjectedUpdate.Source = ProjectedSource;
	ProjectedUpdate.Location = Entry.Location;
	ProjectedUpdate.Scale = Entry.Scale;
	ProjectedUpdate.Bounds = Entry.Bounds;
	ProjectedUpdate.LocalWorldBoxes = {FBox(FVector(4, 4, -100), FVector(6, 6, 100))};
	const TArray<FBox> ProjectedLayerBoxes = ProjectedUpdate.LocalWorldBoxes;
	ProjectedUpdate.Exclusions = {ProjectedUpdate.LocalWorldBoxes[0],
		FBox(FVector(8, 8, -100), FVector(12, 12, 100))};
	auto& ProjectedGroup = ProjectedOwner->Groups.AddDefaulted_GetRef();
	ProjectedGroup.Keys.Add(Surface);
	ProjectedGroup.Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Surface, Module->BlockSize());
	ProjectedGroup.UpdateIndices.Add(0);
	ProjectedGroup.VisibilityActors.Add(Actor);
	ProjectedOwner->Visibility.Add(Actor, true);
	Publisher.Publications.Add(ProjectedOwner);
	SignalFixtureReady(ProjectedOwner);
	const FGuid RestoreGroup = Publisher.BeginGroup(Surface);
	Publisher.SetSharedCoverage(RestoreGroup, Actor, NewShared);
	Publisher.SetHidden(RestoreGroup, Actor, false);
	TestTrue(TEXT("A later show request is retained while an earlier hide is in flight"),
		Publisher.PendingVisibility.Contains(Actor));
	Publisher.SetOwnershipChanged(RestoreGroup, false);
	Publisher.CommitGroup(RestoreGroup);
	for (int32 Tick = 0; Tick < 16; ++Tick)
	{
		TickPublisherFrame(Publisher, *Module);
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	const auto Restoring = Publisher.Publications.FindByPredicate([&](const auto& Candidate)
	{
		return Candidate->Serial > ProjectedOwner->Serial;
	});
	TestTrue(TEXT("A second owner generation prepares while its previous mesh remains pending"),
		Restoring && (*Restoring)->bCoveragePrepared);
	TestTrue(TEXT("Coverage comparison uses the preceding projected state instead of the stale committed state"),
		Restoring && (*Restoring)->Updates.Num() == 1 && !(*Restoring)->Updates[0].bUnchanged);
	ProjectedOwner->PreparedMeshes[0] = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	SignalFixtureReady(ProjectedOwner);
	const double ProjectionDeadline = FPlatformTime::Seconds() + 10.0;
	while (Publisher.IsBusy() && FPlatformTime::Seconds() < ProjectionDeadline)
	{
		TickPublisherFrame(Publisher, *Module);
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestFalse(TEXT("Both projected owner generations finish in order"), Publisher.IsBusy());
	TestTrue(TEXT("The later owner generation restores its requested coverage"), Entry.Exclusions.Contains((*NewShared)[0]));
	TestTrue(TEXT("Shared clipping retains the preceding generation's source and layer exclusions"),
		Entry.PresentedSource == ProjectedSource && Entry.LocalWorldBoxes == ProjectedLayerBoxes);
	TestFalse(TEXT("The later visibility request wins after ordered commits"), Actor->IsHidden());
	const auto EquivalentShared = MakeShared<TArray<FBox>, ESPMode::ThreadSafe>();
	EquivalentShared->Add(FBox(FVector(10, 10, -90), FVector(14, 14, 90)));
	const FGuid EquivalentGroup = Publisher.BeginGroup(Surface);
	Publisher.SetSharedCoverage(EquivalentGroup, Actor, EquivalentShared);
	Publisher.SetOwnershipChanged(EquivalentGroup, false);
	Publisher.CommitGroup(EquivalentGroup);
	const double EquivalentDeadline = FPlatformTime::Seconds() + 10.0;
	while (Publisher.IsBusy() && FPlatformTime::Seconds() < EquivalentDeadline)
	{
		TickPublisherFrame(Publisher, *Module);
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestFalse(TEXT("Exclusion changes outside source bounds reuse the presented geometry"), Publisher.IsBusy());
	TestTrue(TEXT("A geometry reuse still commits the latest exclusion metadata"),
		Entry.Exclusions.Contains((*EquivalentShared)[0]));
	const FVoxelPublishGroupKey Middle{0, {2, 0}, 0}, Edge{0, {3, 0}, 0};
	AActor* ClippedActor = FixtureWorld->SpawnActor<AActor>();
	auto& ClippedEntry = Publisher.Entries.Add(ClippedActor);
	ClippedEntry.GroupKey = Surface;
	ClippedEntry.Scale = Module->BlockSize();
	ClippedEntry.Bounds = FBox(FVector::ZeroVector, FVector(64, 16, 16));
	const auto ClippedOwner = MakeShared<FVoxelViewPublisher::FPublication>();
	ClippedOwner->Serial = ++Publisher.BatchSerial;
	ClippedOwner->bCoveragePrepared = true;
	ClippedOwner->CoverageIndex = 1;
	ClippedOwner->Updates.SetNum(1);
	ClippedOwner->PreparedMeshes.SetNum(1);
	auto& ClipUpdate = ClippedOwner->Updates[0];
	ClipUpdate.Actor = ClippedActor;
	ClipUpdate.Scale = ClippedEntry.Scale;
	ClipUpdate.Bounds = ClippedEntry.Bounds;
	ClipUpdate.bSourceDirty = false;
	ClipUpdate.ChangedBounds = {
		FBox(FVector::ZeroVector, FVector(16, 16, 16) * Module->BlockSize()),
		FBox(FVector(48, 0, 0) * Module->BlockSize(), FVector(64, 16, 16) * Module->BlockSize())};
	ClippedOwner->Callbacks.Add(Surface, {});
	ClippedOwner->Callbacks.Add(Slow, {});
	ClippedOwner->Callbacks.Add(Edge, {});
	ClippedOwner->OwnershipChanges = {Slow, Edge};
	Publisher.BuildGroups(ClippedOwner, true);
	TestEqual(TEXT("Disjoint clipping changes remain one atomic owner group"), ClippedOwner->Groups.Num(), 1);
	Publisher.Publications.Add(ClippedOwner);
	SignalFixtureReady(ClippedOwner);
	const FGuid MiddleGroup = Publisher.BeginGroup(Middle);
	Publisher.CommitGroup(MiddleGroup);
	TickPublisherFrame(Publisher, *Module);
	TestTrue(TEXT("An unprepared clipped owner and its affected Fine edges remain pending"), Publisher.IsGroupBusy(Surface));
	TestFalse(TEXT("Fine between disjoint clipping changes commits without waiting on their aggregate bounds"),
		Publisher.IsGroupBusy(Middle));
	ClippedOwner->PreparedMeshes[0] = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	SignalFixtureReady(ClippedOwner);
	TickPublisherFrame(Publisher, *Module);
	TestFalse(TEXT("Affected edges release together after their clipped owner is ready"), Publisher.IsGroupBusy(Surface));
	TArray<FVoxelViewPublisher::FPublicationRef> SlowGenerations;
	for (int32 Generation = 0; Generation < 2; ++Generation)
	{
		const auto Waiting = MakeShared<FVoxelViewPublisher::FPublication>();
		Waiting->Serial = ++Publisher.BatchSerial;
		Waiting->bCoveragePrepared = true;
		Waiting->CoverageIndex = 1;
		Waiting->Updates.SetNum(1);
		Waiting->Updates[0].bUnchanged = true;
		Waiting->Updates[0].GroupIndex = 0;
		Waiting->PreparedMeshes.SetNum(1);
		auto& WaitingOwner = Waiting->Groups.AddDefaulted_GetRef();
		WaitingOwner.Keys.Add(Slow);
		WaitingOwner.Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Slow, Module->BlockSize());
		WaitingOwner.UpdateIndices.Add(0);
		Publisher.Publications.Add(Waiting);
	SignalFixtureReady(Waiting);
		SlowGenerations.Add(Waiting);
	}
	const FVoxelPublishGroupKey Parent{1, {0, 0}, 1}, Independent{0, {3000, 0}, 0};
	for (const auto& Key : {Slow, Parent, Independent}) Publisher.CommitGroup(Publisher.BeginGroup(Key));
	TickPublisherFrame(Publisher, *Module);
	TestFalse(TEXT("Two unfinished generations of one owner do not block an independent new owner"),
		Publisher.IsGroupBusy(Independent));
	TestTrue(TEXT("A saturated owner is coalesced back into the committed pending queue"),
		Publisher.PendingGroupIds.Contains(Slow) &&
		Publisher.PendingGroups.FindChecked(Publisher.PendingGroupIds.FindChecked(Slow)).bCommitted);
	TestTrue(TEXT("The complete connected parent handoff is deferred with its saturated child"),
		Publisher.PendingGroupIds.Contains(Parent));
	SlowGenerations[0]->PreparedMeshes[0] = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	SignalFixtureReady(SlowGenerations[0]);
	TickPublisherFrame(Publisher, *Module);
	TickPublisherFrame(Publisher, *Module);
	TestFalse(TEXT("The deferred group is admitted once one owner generation finishes"), Publisher.PendingGroupIds.Contains(Slow));
	TestTrue(TEXT("The admitted parent still waits for its older child generation"), Publisher.IsGroupBusy(Parent));
	SlowGenerations[1]->PreparedMeshes[0] = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	SignalFixtureReady(SlowGenerations[1]);
	for (int32 Tick = 0; Tick < 4 && Publisher.IsBusy(); ++Tick) TickPublisherFrame(Publisher, *Module);
	TestFalse(TEXT("All connected owner generations complete in order"), Publisher.IsBusy());
	FVoxelTaskBudget BlockedBudget;
	BlockedBudget.MaxConcurrentTasks = 1;
	BlockedBudget.MaxPendingTasks = 1;
	Scheduler.SetBudget(BlockedBudget);
	for (uint64 Token : {200000ull, 200001ull})
	{
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::GenerateExactBase;
		Request.WorkClass = EVoxelWorkClass::Critical;
		Request.Stamp.WorldEpoch = 1;
		Request.Stamp.Token = Token;
		Request.ReservedBytes = 1024;
		Request.Execute = [](const TAtomic<bool>& Cancel)
		{
			while (!Cancel.Load()) FPlatformProcess::Sleep(0.001f);
			return FVoxelTaskResult();
		};
		TestTrue(TEXT("Critical generation occupies the worker and pending slot"), Scheduler.Enqueue(MoveTemp(Request)));
	}
	AActor* IndependentActor = FixtureWorld->SpawnActor<AActor>();
	auto& IndependentEntry = Publisher.Entries.Add(IndependentActor);
	IndependentEntry.GroupKey = Distant;
	IndependentEntry.Source = IndependentEntry.PresentedSource = CommittedSource;
	IndependentEntry.bDirty = false;
	const auto BlockedPublication = MakeShared<FVoxelViewPublisher::FPublication>();
	BlockedPublication->Serial = ++Publisher.BatchSerial;
	BlockedPublication->bCoveragePrepared = true;
	BlockedPublication->CoverageIndex = 2;
	BlockedPublication->Updates.SetNum(2);
	BlockedPublication->PreparedMeshes.SetNum(2);
	auto& BlockedUpdate = BlockedPublication->Updates[0];
	BlockedUpdate.Actor = Actor;
	BlockedUpdate.Source = ProjectedSource;
	BlockedUpdate.Exclusions = {FBox(FVector(12, 12, -100), FVector(16, 16, 100))};
	BlockedUpdate.GroupIndex = 0;
	BlockedUpdate.TerrainStage = 2;
	auto& IndependentUpdate = BlockedPublication->Updates[1];
	IndependentUpdate.Actor = IndependentActor;
	IndependentUpdate.Source = CommittedSource;
	IndependentUpdate.GroupIndex = 1;
	IndependentUpdate.bUnchanged = true;
	BlockedPublication->Groups.SetNum(2);
	BlockedPublication->Groups[0].Keys.Add(Surface);
	BlockedPublication->Groups[0].Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Surface, Module->BlockSize());
	BlockedPublication->Groups[0].UpdateIndices.Add(0);
	BlockedPublication->Groups[1].Keys.Add(Distant);
	BlockedPublication->Groups[1].Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Distant, Module->BlockSize());
	BlockedPublication->Groups[1].UpdateIndices.Add(1);
	Publisher.Publications.Add(BlockedPublication);
	SignalFixtureReady(BlockedPublication);
	TickPublisherFrame(Publisher, *Module);
	TestFalse(TEXT("An unadmitted clipping task retains its own pending group"), BlockedPublication->Groups[0].bCommitted);
	TestTrue(TEXT("An unadmitted clipping task does not block a later ready independent group"),
		BlockedPublication->Groups[1].bCommitted);
	Scheduler.StopAndJoin();
	Publisher.Reset();
	const TArray<TWeakObjectPtr<AActor>> PendingActors{Actor, IndependentActor, ClippedActor};
	TArray<FGuid> PendingHandles;
	for (int32 Index = 0; Index < PendingActors.Num(); ++Index)
	{
		const FVoxelPublishGroupKey Key{0, FIntPoint(4000 + Index * 1000, 0), 0};
		auto& PendingEntry = Publisher.Entries.Add(PendingActors[Index]);
		PendingEntry.GroupKey = Key;
		PendingEntry.Source = CommittedSource;
		const FGuid Handle = Publisher.BeginGroup(Key);
		PendingHandles.Add(Handle);
		Publisher.SetCoverage(Handle, PendingActors[Index].Get(), {FBox(FVector::ZeroVector, FVector(1))});
	}
	Publisher.SetCoverage(PendingHandles[0], Actor, {FBox(FVector::ZeroVector, FVector(2))});
	TestEqual(TEXT("Replacing pending input keeps one update per actor"), Publisher.PendingUpdates.Num(), 3);
	Publisher.Forget(IndependentActor);
	TestFalse(TEXT("Forget removes the corresponding pending lookup"), Publisher.PendingUpdateIndices.Contains(PendingActors[1]));
	TestEqual(TEXT("Removing a pending actor repairs the swapped actor index"), Publisher.PendingUpdateIndices.FindChecked(PendingActors[2]), 1);
	Publisher.SetCoverage(PendingHandles[2], ClippedActor, {});
	TestEqual(TEXT("A repaired index replaces input without appending another actor"), Publisher.PendingUpdates.Num(), 2);
	Publisher.TakePendingUpdate(0);
	TestEqual(TEXT("Moving input to an immutable publication repairs the remaining pending index"),
		Publisher.PendingUpdateIndices.FindChecked(PendingActors[2]), 0);
	Publisher.Reset();
	TestTrue(TEXT("Reset clears the pending input lookup"), Publisher.PendingUpdateIndices.IsEmpty());
	auto& StableEntry = Publisher.Entries.Add(Actor);
	StableEntry.GroupKey = Surface;
	StableEntry.Source = StableEntry.PresentedSource = CommittedSource;
	StableEntry.bDirty = false;
	StableEntry.Scale = Module->BlockSize();
	StableEntry.Bounds = FBox(FVector::ZeroVector, FVector(32));
	const TArray<FBox> StableBoxes{FBox(FVector(1), FVector(2))};
	StableEntry.Exclusions = StableBoxes;
	FGuid StableHandle = Publisher.BeginGroup(Surface);
	for (int32 Iteration = 0; Iteration < 64; ++Iteration)
		Publisher.SetCoverage(StableHandle, Actor, StableBoxes);
	TestTrue(TEXT("Repeated committed coverage does not enqueue comparison work"), Publisher.PendingUpdates.IsEmpty());
	Publisher.SetOwnershipChanged(StableHandle, false);
	TestTrue(TEXT("An unchanged owner does not enter publication planning"), Publisher.DiscardUnchangedGroup(StableHandle));
	TestFalse(TEXT("Discarding unchanged input releases its owner"), Publisher.IsGroupBusy(Surface));
	StableHandle = Publisher.BeginGroup(Surface);
	Publisher.SetOwnershipChanged(StableHandle, true);
	TestFalse(TEXT("Empty geometry still publishes an ownership change"), Publisher.DiscardUnchangedGroup(StableHandle));
	Publisher.SetOwnershipChanged(StableHandle, false);
	const TArray<FBox> ChangedBoxes{FBox(FVector(3), FVector(4))};
	StableEntry.Source = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	StableEntry.bDirty = true;
	Publisher.SetCoverage(StableHandle, Actor, ChangedBoxes);
	TestEqual(TEXT("A real exclusion change queues one update"), Publisher.PendingUpdates.Num(), 1);
	TestFalse(TEXT("Changed coverage is retained for publication"), Publisher.DiscardUnchangedGroup(StableHandle));
	Publisher.CommitGroup(StableHandle);
	for (int32 Step = 0; Step < 1024 && (Publisher.PendingPlan || !Publisher.PendingGroups.IsEmpty()); ++Step)
		Publisher.AdvancePendingPlan();
	const FGuid FollowingHandle = Publisher.BeginGroup(Surface);
	Publisher.SetCoverage(FollowingHandle, Actor, ChangedBoxes);
	TestTrue(TEXT("Identical in-flight geometry and coverage do not create a second comparison"), Publisher.PendingUpdates.IsEmpty());
	Publisher.SetOwnershipChanged(FollowingHandle, false);
	TestFalse(TEXT("An outstanding generation retains owner ordering"), Publisher.DiscardUnchangedGroup(FollowingHandle));
	Publisher.SetCoverage(FollowingHandle, Actor, StableBoxes);
	TestEqual(TEXT("Restoring committed coverage while a change is in flight still queues the restoration"), Publisher.PendingUpdates.Num(), 1);
	const auto UnchangedPublication = MakeShared<FVoxelViewPublisher::FPublication>();
	auto& UnchangedUpdate = UnchangedPublication->Updates.AddDefaulted_GetRef();
	UnchangedUpdate.Actor = Actor;
	UnchangedUpdate.Source = StableEntry.Source;
	UnchangedUpdate.Location = StableEntry.Location;
	UnchangedUpdate.Scale = StableEntry.Scale;
	UnchangedUpdate.bUnchanged = true;
	StableEntry.PresentedSource = StableEntry.Source;
	auto& UnchangedGroup = UnchangedPublication->Groups.AddDefaulted_GetRef();
	UnchangedGroup.UpdateIndices.Add(0);
	Publisher.CommitReadyGroup(UnchangedPublication, UnchangedGroup);
	TestFalse(TEXT("An unchanged committed generation clears its dirty state"), StableEntry.bDirty);
	StableEntry.Source = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	Publisher.CommitReadyGroup(UnchangedPublication, UnchangedGroup);
	TestTrue(TEXT("An older unchanged commit retains a newer source as dirty"), StableEntry.bDirty);
	Publisher.Reset();
	FVoxelTaskScheduler BatchScheduler(true);
	if (!TestTrue(TEXT("发布夹具初始化真实控制身份与统计账本"), BatchScheduler.InitializeStatistics(1))) return false;
	FVoxelViewPublisher BatchPublisher(*Module, BatchScheduler, 1);
	const auto Batched = MakeShared<FVoxelViewPublisher::FPublication>();
	Batched->Serial = ++BatchPublisher.BatchSerial;
	Batched->Groups.AddDefaulted_GetRef().Keys.Add(Surface);
	for (int32 Index = 0; Index < 32; ++Index)
	{
		auto& Update = Batched->Updates.AddDefaulted_GetRef();
		Update.GroupIndex = 0;
		Update.Scale = Module->BlockSize();
		Update.Bounds = FBox(FVector::ZeroVector, FVector(32));
		Update.Exclusions = ChangedBoxes;
		Batched->Groups[0].UpdateIndices.Add(Index);
	}
	BatchPublisher.AdmitCoveragePreparation(Batched);
	const double BatchDeadline = FPlatformTime::Seconds() + 10.0;
	while (Batched->bCoveragePreparing && FPlatformTime::Seconds() < BatchDeadline)
	{
		BatchScheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestEqual(TEXT("One worker round trip respects the configured comparison batch"), Batched->CoverageIndex,
		Module->GetViewSettings().Budget.CoveragePreparationBatchSize);
	while (Batched->CoverageIndex < Batched->Updates.Num() && FPlatformTime::Seconds() < BatchDeadline)
	{
		BatchPublisher.AdmitCoveragePreparation(Batched);
		BatchScheduler.Tick(
			[](FVoxelTaskResult&&)
			{
			},
			8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(
		TEXT("Successive bounded batches finish every independent update"), Batched->bCoveragePrepared && Batched->CoverageIndex == Batched->Updates.Num());
	BatchScheduler.StopAndJoin();
	Publisher.Reset();
	const auto Prioritized = MakeShared<FVoxelViewPublisher::FPublication>();
	Prioritized->Groups.SetNum(3);
	const double BlockSize = Module->BlockSize();
	Prioritized->Groups[0].Keys.Add({3, FIntPoint::ZeroValue, 0});
	Prioritized->Groups[0].Bounds = FBox(FVector(50, 50, 0) * BlockSize, FVector(60, 60, 1) * BlockSize);
	Prioritized->Groups[0].ChangedBounds.Add(Prioritized->Groups[0].Bounds);
	Prioritized->Groups[1].Keys.Add(Slow);
	Prioritized->Groups[1].Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(Slow, BlockSize);
	Prioritized->Groups[2].Keys.Add({3, FIntPoint::ZeroValue, 1});
	Prioritized->Groups[2].ChangedBounds = {
		FBox(FVector(-100, 0, 0) * BlockSize, FVector(-90, 16, 1) * BlockSize),
		FBox(FVector(90, 0, 0) * BlockSize, FVector(100, 16, 1) * BlockSize)};
	Prioritized->Groups[2].Bounds = FBox(FVector(-100, 0, 0) * BlockSize, FVector(100, 16, 1) * BlockSize);
	const TArray<FVector> PriorityObservers{FVector::ZeroVector};
	Publisher.SetObservers(PriorityObservers);
	TestTrue(TEXT("A distant clipping change is farther than nearby Fine even when its coarse owner contains the observer"),
		Publisher.GroupDistance(Prioritized->Groups[0]) > Publisher.GroupDistance(Prioritized->Groups[1]));
	TestEqual(TEXT("Disjoint changes use their actual distance instead of the aggregate bounds across the observer"),
		Publisher.GroupDistance(Prioritized->Groups[2]), FMath::Square(90.0 * BlockSize));
	auto& PrioritySource = const_cast<FVoxelInterestSet&>(Module->GetCurrentInterest()).Sources.Add(FGuid::NewGuid()).Source;
	PrioritySource.Capabilities = EVoxelStreamingCapability::FineVisual | EVoxelStreamingCapability::WorldVisual;
	PrioritySource.Center = FIntVector(0, 0, 8);
	PrioritySource.SchedulingPriority = 7;
	FVoxelTaskRequest FarPriority;
	FarPriority.WorkClass = EVoxelWorkClass::Visible;
	const int32 FarGroupIndex = 0;
	Publisher.SetPublicationPriority(FarPriority, *Prioritized, MakeArrayView(&FarGroupIndex, 1));
	TestEqual(TEXT("Distant clipping does not acquire the Critical lane through a large owner footprint"),
		FarPriority.WorkClass, EVoxelWorkClass::Visible);
	TestTrue(TEXT("Publication worker distance uses actual world bounds converted to cells"),
		FMath::IsNearlyEqual(FarPriority.DistanceScore, FMath::Sqrt(5000.0)));
	TestEqual(TEXT("Actual clipping preserves its visual source scheduling priority"), FarPriority.SourcePriority, 7);
	FVoxelTaskRequest NearPriority;
	NearPriority.WorkClass = EVoxelWorkClass::Visible;
	const int32 NearGroupIndex = 1;
	Publisher.SetPublicationPriority(NearPriority, *Prioritized, MakeArrayView(&NearGroupIndex, 1));
	TestEqual(TEXT("Near Fine publication uses the actual three-dimensional source distance"),
		NearPriority.WorkClass, EVoxelWorkClass::Critical);
	Prioritized->Groups[1].Bounds = FBox(ForceInit);
	TestEqual(TEXT("Metadata-only logical ownership still receives its owner distance"),
		Publisher.GroupDistance(Prioritized->Groups[1]), 0.0);
	Publisher.SortGroups(Prioritized);
	TestTrue(TEXT("Actual-change sorting places nearby Fine ahead of a distant coarse update"),
		Prioritized->Groups[0].Keys[0] == Slow);
	const_cast<FVoxelInterestSet&>(Module->GetCurrentInterest()).Sources.Reset();
	const auto MovingPublication = MakeShared<FVoxelViewPublisher::FPublication>();
	MovingPublication->Serial = ++Publisher.BatchSerial;
	MovingPublication->bCoveragePrepared = true;
	MovingPublication->CoverageIndex = 2;
	MovingPublication->Groups.SetNum(2);
	MovingPublication->Updates.SetNum(2);
	MovingPublication->PreparedMeshes.Init(CommittedSource, 2);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		MovingPublication->Groups[Index].Keys.Add(Index == 0 ? Slow : Distant);
		MovingPublication->Groups[Index].Bounds = FVoxelPublishGroupPlanner::OwnershipBounds(
			MovingPublication->Groups[Index].Keys[0], BlockSize);
		MovingPublication->Groups[Index].UpdateIndices.Add(Index);
		MovingPublication->Updates[Index].GroupIndex = Index;
	}
	Publisher.SortGroups(MovingPublication);
	Publisher.Publications.Add(MovingPublication);
	SignalFixtureReady(MovingPublication);
	auto& MovingSettings = const_cast<FVoxelViewSettings&>(Module->GetViewSettings());
	const int32 PreviousGroupLimit = MovingSettings.Budget.MaxPublishGroupsPerFrame;
	MovingSettings.Budget.MaxPublishGroupsPerFrame = 1;
	const TArray<FVector> MovedObservers{FVector(16000.0 * BlockSize, 0, 0)};
	Publisher.SetObservers(MovedObservers);
	TickPublisherFrame(Publisher, *Module);
	TestTrue(TEXT("A ready group near the moved observer commits within the unchanged one-group frame limit"),
		MovingPublication->Groups[1].bCommitted);
	TestFalse(TEXT("The old observer's independent group waits for the next frame"), MovingPublication->Groups[0].bCommitted);
	TestTrue(TEXT("Moving priority retains stable in-flight update and group indices"),
		MovingPublication->Updates[0].GroupIndex == 0 && MovingPublication->Updates[1].GroupIndex == 1);
	Publisher.RefreshGroupOrder(MovingPublication);
	TestTrue(TEXT("The derived active order removes committed groups without compacting stamped updates"),
		MovingPublication->GroupOrder == TArray<int32>{0} && MovingPublication->Updates.Num() == 2);
	Publisher.SetObservers(PriorityObservers);
	TickPublisherFrame(Publisher, *Module);
	TestFalse(TEXT("Both independently prioritized groups eventually finish"), Publisher.IsBusy());
	auto ReadyPublication = [&](TArray<FVoxelPublishGroupKey> Keys, const FBox& Bounds)
	{
		const auto Ready = MakeShared<FVoxelViewPublisher::FPublication>();
		Ready->Serial = ++Publisher.BatchSerial;
		Ready->bCoveragePrepared = true;
		Ready->CoverageIndex = 1;
		Ready->Updates.SetNum(1);
		Ready->Updates[0].GroupIndex = 0;
		Ready->PreparedMeshes.Init(CommittedSource, 1);
		auto& ReadyGroup = Ready->Groups.AddDefaulted_GetRef();
		ReadyGroup.Keys = MoveTemp(Keys);
		ReadyGroup.Bounds = Bounds;
		ReadyGroup.UpdateIndices.Add(0);
		Publisher.Publications.Add(Ready);
	SignalFixtureReady(Ready);
		return Ready;
	};
	auto& InheritedSource = const_cast<FVoxelInterestSet&>(Module->GetCurrentInterest()).Sources.Add(FGuid::NewGuid()).Source;
	InheritedSource.Capabilities = EVoxelStreamingCapability::FineVisual | EVoxelStreamingCapability::WorldVisual;
	InheritedSource.Center = FIntVector(16, 0, 0);
	InheritedSource.SchedulingPriority = 9;
	const auto RemoteDependency = ReadyPublication({Surface},
		FBox(FVector(1000, 0, 0) * BlockSize, FVector(1016, 16, 1) * BlockSize));
	const auto NearbyDependent = ReadyPublication({Surface, Slow},
		FBox(FVector::ZeroVector, FVector(16, 16, 1) * BlockSize));
	const auto NearbyIndependent = ReadyPublication({{0, {1, 0}, 0}},
		FBox(FVector(16, 0, 0) * BlockSize, FVector(32, 16, 1) * BlockSize));
	const TArray<FVector> BoundaryObservers{FVector(16 * BlockSize, 0, 0)};
	Publisher.SetObservers(BoundaryObservers);
	const int32 InheritedGroupIndex = 0;
	FVoxelTaskRequest BeforeInheritance;
	BeforeInheritance.WorkClass = EVoxelWorkClass::Visible;
	Publisher.SetPublicationPriority(BeforeInheritance, *RemoteDependency, MakeArrayView(&InheritedGroupIndex, 1));
	TestEqual(TEXT("A remote dependency starts in the Visible worker lane before nearby demand"),
		BeforeInheritance.WorkClass, EVoxelWorkClass::Visible);
	TickPublisherFrame(Publisher, *Module);
	TestTrue(TEXT("A distant earlier owner inherits nearby handoff priority and completes within one-group budget"),
		RemoteDependency->Groups[0].bCommitted);
	TestEqual(TEXT("The old dependency receives the waiting near group's effective distance"),
		RemoteDependency->Groups[0].Priority.ObserverDistance, NearbyDependent->Groups[0].Priority.ObserverDistance);
	TestFalse(TEXT("Independent nearby work cannot consume the frame slot needed by an older handoff dependency"),
		NearbyIndependent->Groups[0].bCommitted);
	FVoxelTaskRequest AfterInheritance;
	AfterInheritance.WorkClass = EVoxelWorkClass::Visible;
	Publisher.SetPublicationPriority(AfterInheritance, *RemoteDependency, MakeArrayView(&InheritedGroupIndex, 1));
	TestEqual(TEXT("A real old owner also inherits the waiting Fine group's Critical worker lane"),
		AfterInheritance.WorkClass, EVoxelWorkClass::Critical);
	TestEqual(TEXT("Worker inheritance preserves the waiting source scheduling priority"), AfterInheritance.SourcePriority, 9);
	TestEqual(TEXT("Worker inheritance uses the waiting actual change distance in cells"), AfterInheritance.DistanceScore, 0.0);
	for (int32 Frame = 0; Frame < 4 && Publisher.IsBusy(); ++Frame) TickPublisherFrame(Publisher, *Module);
	TestTrue(TEXT("The nearby dependent eventually commits after its earlier owner"), NearbyDependent->Groups[0].bCommitted);
	TestTrue(TEXT("Independent nearby work still eventually completes"), NearbyIndependent->Groups[0].bCommitted);
	const FBox NearChange(FVector::ZeroVector, FVector(16, 16, 1) * BlockSize);
	const FBox FarChange(FVector(1000, 0, 0) * BlockSize, FVector(1016, 16, 1) * BlockSize);
	const auto SpatialDependency = ReadyPublication({{3, {1, 0}, 0}}, FarChange);
	const auto SparseDependent = ReadyPublication({Surface, Slow}, NearChange + FarChange);
	SparseDependent->Groups[0].ChangedBounds = {NearChange, FarChange};
	const auto MiddleIndependent = ReadyPublication({{0, {1, 0}, 0}},
		FBox(FVector(16, 0, 0) * BlockSize, FVector(32, 16, 1) * BlockSize));
	TestTrue(TEXT("Different owners still share a dependency where their actual changed regions overlap"),
		Publisher.GroupsInteract(SparseDependent->Groups[0], SpatialDependency->Groups[0]));
	TestFalse(TEXT("Empty middle space inside sparse aggregate bounds is not a spatial dependency"),
		Publisher.GroupsInteract(SparseDependent->Groups[0], MiddleIndependent->Groups[0]));
	TickPublisherFrame(Publisher, *Module);
	TestTrue(TEXT("A distant spatial dependency inherits nearby urgency without shared owner keys"),
		SpatialDependency->Groups[0].bCommitted);
	TestEqual(TEXT("Spatial priority propagation uses the dependent's actual sparse change distance"),
		SpatialDependency->Groups[0].Priority.ObserverDistance, SparseDependent->Groups[0].Priority.ObserverDistance);
	FVoxelTaskRequest SpatialWorkerPriority;
	Publisher.SetPublicationPriority(SpatialWorkerPriority, *SpatialDependency, MakeArrayView(&InheritedGroupIndex, 1));
	TestEqual(TEXT("A spatial dependency with different owners inherits Critical for its worker too"),
		SpatialWorkerPriority.WorkClass, EVoxelWorkClass::Critical);
	TestFalse(TEXT("The unchanged one-group limit still defers independent nearby work"), MiddleIndependent->Groups[0].bCommitted);
	for (int32 Frame = 0; Frame < 4 && Publisher.IsBusy(); ++Frame) TickPublisherFrame(Publisher, *Module);
	TestTrue(TEXT("The sparse dependent commits after its actual spatial dependency"), SparseDependent->Groups[0].bCommitted);
	TestTrue(TEXT("The unrelated middle group also eventually commits"), MiddleIndependent->Groups[0].bCommitted);
	const auto FirstDeferredOwner = ReadyPublication({Surface}, FarChange);
	const auto SecondDeferredOwner = ReadyPublication({Surface}, FarChange);
	FirstDeferredOwner->PreparedMeshes[0].Reset();
	SecondDeferredOwner->PreparedMeshes[0].Reset();
	const auto DeferredNearby = ReadyPublication({Surface, Slow}, NearChange);
	for (const auto& ActivePublication : Publisher.Publications) Publisher.RefreshGroupOrder(ActivePublication);
	TMap<FVoxelPublishGroupKey, FVoxelViewPublisher::FGroupPriority> OneFrameDonations;
	Publisher.DeferSaturatedGroups(DeferredNearby, OneFrameDonations);
	Publisher.Publications.Remove(DeferredNearby);
	TestTrue(TEXT("A saturated group retains its actual priority bounds in pending ownership"),
		Publisher.PendingGroups.FindChecked(Publisher.PendingGroupIds.FindChecked(Surface)).DeferredPriorityBounds.IsValid());
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		Publisher.UpdateGroupPriorities({});
		TestEqual(TEXT("Pending actual demand keeps older dependencies urgent beyond its one-frame donation"),
			FirstDeferredOwner->Groups[0].Priority.ObserverDistance, 0.0);
		TestEqual(TEXT("Pending actual demand keeps worker urgency beyond its one-frame donation"),
			FirstDeferredOwner->Groups[0].Priority.WorkClass, EVoxelWorkClass::Critical);
	}
	Publisher.StartCommittedGroups();
	Publisher.UpdateGroupPriorities({});
	TestTrue(TEXT("Saturated ownership retains actual urgency without starting a third generation"),
		Publisher.PendingGroupIds.Contains(Surface) &&
		Publisher.PendingGroups.FindChecked(Publisher.PendingGroupIds.FindChecked(Surface)).DeferredPriorityBounds.IsValid() &&
		FirstDeferredOwner->Groups[0].Priority.ObserverDistance == 0.0);
	const TArray<FVector> AwayObservers{FVector(900 * BlockSize, 0, 0)};
	InheritedSource.Center.X = 900;
	Publisher.SetObservers(AwayObservers);
	Publisher.UpdateGroupPriorities({});
	TestTrue(TEXT("Deferred urgency is recomputed from current observers instead of retaining a stale zero distance"),
		FirstDeferredOwner->Groups[0].Priority.ObserverDistance > 0.0);
	TestEqual(TEXT("Moving the source removes stale Critical worker urgency"),
		FirstDeferredOwner->Groups[0].Priority.WorkClass, EVoxelWorkClass::Visible);
	Publisher.DiscardGroups();
	InheritedSource.Center.X = 16;
	FVoxelTaskScheduler InheritanceScheduler(true);
	if (!TestTrue(TEXT("发布夹具初始化真实控制身份与统计账本"), InheritanceScheduler.InitializeStatistics(1))) return false;
	FVoxelTaskBudget InheritanceBudget;
	InheritanceBudget.MaxConcurrentTasks = 1;
	InheritanceBudget.CriticalReservedTasks = 0;
	InheritanceBudget.MaxPendingTasks = 8;
	InheritanceScheduler.SetBudget(InheritanceBudget);
	FVoxelViewPublisher TaskPublisher(*Module, InheritanceScheduler, 1);
	TaskPublisher.SetObservers(BoundaryObservers);
	const auto MakeTaskPublication = [&](TArray<FVoxelPublishGroupKey> Keys, const FBox& Bounds)
	{
		const auto TaskPublication = MakeShared<FVoxelViewPublisher::FPublication>();
		TaskPublication->Serial = ++TaskPublisher.BatchSerial;
		TaskPublication->bCoveragePrepared = true;
		TaskPublication->CoverageIndex = 1;
		TaskPublication->Updates.SetNum(1);
		TaskPublication->Updates[0].GroupIndex = 0;
		auto& TaskGroup = TaskPublication->Groups.AddDefaulted_GetRef();
		TaskGroup.Keys = MoveTemp(Keys);
		TaskGroup.Bounds = Bounds;
		TaskGroup.UpdateIndices.Add(0);
		TaskPublisher.RefreshGroupOrder(TaskPublication);
		TaskPublisher.Publications.Add(TaskPublication);
		for (const auto& Key : TaskGroup.Keys) TaskPublisher.SetOwnerActive(Key, TaskPublication->Serial, true);
		return TaskPublication;
	};
	const auto WorkerDependency = MakeTaskPublication({Surface}, FarChange);
	MakeTaskPublication({Surface, Slow}, NearChange);
	TAtomic<bool> ReleaseWorker {false};
	FVoxelTaskRequest HoldWorker;
	HoldWorker.Kind = EVoxelTaskKind::BuildCollision;
	HoldWorker.WorkClass = EVoxelWorkClass::Critical;
	HoldWorker.Stamp.WorldEpoch = 1;
	HoldWorker.Stamp.Token = 100;
	HoldWorker.ReservedBytes = 1024;
	HoldWorker.Execute = [&ReleaseWorker](const TAtomic<bool>& Cancel)
	{
		while (!ReleaseWorker.Load() && !Cancel.Load()) FPlatformProcess::Sleep(0.001f);
		FVoxelTaskResult Result;
		Result.bSuccess = true;
		return Result;
	};
	TestTrue(TEXT("A real single worker is held while dependency priorities are refreshed"),
		InheritanceScheduler.Enqueue(MoveTemp(HoldWorker)));
	InheritanceScheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
	TestEqual(TEXT("The held task occupies the actual single worker"), InheritanceScheduler.GetDiagnostics().Running, 1);
	TArray<int32> WorkerOrder;
	FVoxelTaskRequest OldClipping;
	OldClipping.Kind = EVoxelTaskKind::BuildViewCoverage;
	OldClipping.bPublicationContinuation = true;
	OldClipping.WorkClass = EVoxelWorkClass::Visible;
	OldClipping.Stamp.WorldEpoch = 1;
	OldClipping.Stamp.Section = FIntVector(MIN_int32, 3, 0);
	OldClipping.Stamp.Token = WorkerDependency->Serial;
	OldClipping.ReservedBytes = 1024;
	TaskPublisher.SetPublicationPriority(OldClipping, *WorkerDependency, MakeArrayView(&InheritedGroupIndex, 1));
	OldClipping.Execute = [&WorkerOrder](const TAtomic<bool>&)
	{
		WorkerOrder.Add(1);
		FVoxelTaskResult Result;
		Result.bSuccess = true;
		return Result;
	};
	TestTrue(TEXT("The remote publication clipping continuation enters the real queue"),
		InheritanceScheduler.Enqueue(MoveTemp(OldClipping)));
	FVoxelTaskRequest NewFineWork;
	NewFineWork.Kind = EVoxelTaskKind::BuildFineMesh;
	NewFineWork.WorkClass = EVoxelWorkClass::Critical;
	NewFineWork.SourcePriority = 9;
	NewFineWork.Stamp.WorldEpoch = 1;
	NewFineWork.Stamp.Token = 101;
	NewFineWork.ReservedBytes = 1024;
	NewFineWork.Execute = [&WorkerOrder](const TAtomic<bool>&)
	{
		WorkerOrder.Add(2);
		FVoxelTaskResult Result;
		Result.bSuccess = true;
		return Result;
	};
	TestTrue(TEXT("New Critical Fine competes with the remote dependency in the same real queue"),
		InheritanceScheduler.Enqueue(MoveTemp(NewFineWork)));
	FVoxelTaskRequest HigherSourceWork;
	HigherSourceWork.Kind = EVoxelTaskKind::BuildFineMesh;
	HigherSourceWork.WorkClass = EVoxelWorkClass::Critical;
	HigherSourceWork.SourcePriority = 8;
	HigherSourceWork.Stamp.WorldEpoch = 1;
	HigherSourceWork.Stamp.Token = 102;
	HigherSourceWork.ReservedBytes = 1024;
	HigherSourceWork.Execute = [&WorkerOrder](const TAtomic<bool>&)
	{
		WorkerOrder.Add(3);
		FVoxelTaskResult Result;
		Result.bSuccess = true;
		return Result;
	};
	TestTrue(TEXT("An unrelated higher-priority source competes without inheriting publication metadata"),
		InheritanceScheduler.Enqueue(MoveTemp(HigherSourceWork)));
	TaskPublisher.UpdateGroupPriorities({});
	TaskPublisher.RefreshTaskPriorities();
	TestEqual(TEXT("The existing queued continuation inherits Critical instead of waiting behind new Fine"),
		InheritanceScheduler.GetDiagnostics().CriticalPending, 3);
	ReleaseWorker.Store(true);
	const double WorkerInheritanceDeadline = FPlatformTime::Seconds() + 5.0;
	while (InheritanceScheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < WorkerInheritanceDeadline)
	{
		InheritanceScheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	InheritanceScheduler.StopAndJoin();
	TestTrue(TEXT("Actual worker execution preserves higher sources then releases the inherited clipping before same-source Fine"),
		WorkerOrder == TArray<int32>{3, 1, 2});
	FVoxelTaskScheduler AdmissionScheduler(true);
	if (!TestTrue(TEXT("发布夹具初始化真实控制身份与统计账本"), AdmissionScheduler.InitializeStatistics(1))) return false;
	AdmissionScheduler.SetBudget(InheritanceBudget);
	TAtomic<bool> ReleaseAdmissionWorker {false};
	FVoxelTaskRequest HoldAdmissionWorker;
	HoldAdmissionWorker.Kind = EVoxelTaskKind::BuildCollision;
	HoldAdmissionWorker.WorkClass = EVoxelWorkClass::Critical;
	HoldAdmissionWorker.Stamp.WorldEpoch = 1;
	HoldAdmissionWorker.Stamp.Token = 1000;
	HoldAdmissionWorker.ReservedBytes = 1024;
	HoldAdmissionWorker.Execute = [&ReleaseAdmissionWorker](const TAtomic<bool>& Cancel)
	{
		while (!ReleaseAdmissionWorker.Load() && !Cancel.Load()) FPlatformProcess::Sleep(0.001f);
		FVoxelTaskResult Result;
		Result.bSuccess = true;
		return Result;
	};
	TestTrue(TEXT("A held real worker makes clipping admission and cancellation observable in the queue"),
		AdmissionScheduler.Enqueue(MoveTemp(HoldAdmissionWorker)));
	TestEqual(TEXT("The admission fixture occupies its sole worker"), AdmissionScheduler.GetDiagnostics().Running, 1);
	FVoxelViewPublisher AdmissionPublisher(*Module, AdmissionScheduler, 1);
	AdmissionPublisher.SetObservers(BoundaryObservers);
	const auto AdmissionPublication = MakeShared<FVoxelViewPublisher::FPublication>();
	AdmissionPublication->Serial = ++AdmissionPublisher.BatchSerial;
	AdmissionPublication->bCoveragePrepared = true;
	constexpr int32 DistantUpdates = 140;
	AdmissionPublication->CoverageIndex = DistantUpdates + 1;
	AdmissionPublication->Updates.SetNum(DistantUpdates + 1);
	AdmissionPublication->PreparedMeshes.SetNum(DistantUpdates + 1);
	AdmissionPublication->Groups.SetNum(2);
	AdmissionPublication->Groups[0].Keys.Add(Surface);
	AdmissionPublication->Groups[0].Bounds = FarChange;
	AdmissionPublication->Groups[1].Keys.Add(Slow);
	AdmissionPublication->Groups[1].Bounds = NearChange;
	const auto AdmissionMesh = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	for (int32 Index = 0; Index <= DistantUpdates; ++Index)
	{
		auto& Update = AdmissionPublication->Updates[Index];
		Update.GroupIndex = Index == DistantUpdates ? 1 : 0;
		Update.TerrainStage = Index == DistantUpdates ? 0 : 2;
		Update.Source = AdmissionMesh;
		Update.Exclusions.Add(FBox(FVector::ZeroVector, FVector(1, 1, 1)));
		AdmissionPublication->Groups[Update.GroupIndex].UpdateIndices.Add(Index);
	}
	AdmissionPublisher.RefreshGroupOrder(AdmissionPublication);
	AdmissionPublisher.Publications.Add(AdmissionPublication);
	AdmissionPublisher.UpdateGroupPriorities({});
	do { AdmissionPublisher.AdmitBuilds(AdmissionPublication); } while (!AdmissionPublication->bBuildOrderInitialized || AdmissionPublication->bBuildOrderDirty);
	TestTrue(TEXT("The current Critical clipping is admitted ahead of 140 physically earlier distant updates"),
		AdmissionPublication->PendingBuildIndices.Contains(DistantUpdates));
	TestEqual(TEXT("Priority admission retains the four in-flight clipping limit"),
		AdmissionPublication->PendingBuildIndices.Num(), 4);
	TestEqual(TEXT("The actual admitted queue contains the movement-critical clipping"),
		AdmissionScheduler.GetDiagnostics().CriticalPending, 1);
	AdmissionScheduler.CancelMatching([](EVoxelTaskKind Kind, const FVoxelTaskStamp&)
	{
		return Kind == EVoxelTaskKind::BuildViewCoverage;
	});
	AdmissionScheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
	TestTrue(TEXT("Canceled admitted clipping releases slots without discarding the immutable publication"),
		AdmissionPublication->PendingBuildIndices.IsEmpty() && !AdmissionPublication->bCanceled);
	for (int32 Index = 0; Index < DistantUpdates; ++Index)
		AdmissionPublication->Updates[Index].bUnchanged = true;
	do { AdmissionPublisher.AdmitBuilds(AdmissionPublication); } while (!AdmissionPublication->bBuildOrderInitialized || AdmissionPublication->bBuildOrderDirty);
	TestTrue(TEXT("A retried Critical clipping remains ahead of more than one frame of unchanged distant updates"),
		AdmissionPublication->PendingBuildIndices.Contains(DistantUpdates));
	int32 InspectedMeshes = 0;
	for (const auto& Mesh : AdmissionPublication->PreparedMeshes) InspectedMeshes += Mesh.IsValid();
	TestEqual(
		TEXT("Priority admission respects the configured inspection batch"), InspectedMeshes, Module->GetViewSettings().Budget.PublishAdmissionBatchSize - 1);
	TestTrue(TEXT("Clipping admission preserves the accepted physical update and task indices"),
		AdmissionPublication->Updates[DistantUpdates].GroupIndex == 1 &&
		AdmissionPublication->Updates[DistantUpdates].Source == AdmissionMesh);
	ReleaseAdmissionWorker.Store(true);
	const double AdmissionApplyDeadline = FPlatformTime::Seconds() + 5.0;
	while (AdmissionScheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < AdmissionApplyDeadline)
	{
		AdmissionScheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("The retried Critical clipping executes and applies to its original accepted update"),
		AdmissionPublication->PreparedMeshes[DistantUpdates].IsValid() &&
		AdmissionPublication->PendingBuildIndices.IsEmpty());
	AdmissionScheduler.StopAndJoin();
	InheritedSource.Capabilities = EVoxelStreamingCapability::FineVisual | EVoxelStreamingCapability::LocalRefinement;
	TaskPublisher.UpdateGroupPriorities({});
	FVoxelTaskRequest LocalWorkerPriority;
	TaskPublisher.SetPublicationPriority(LocalWorkerPriority, *WorkerDependency, MakeArrayView(&InheritedGroupIndex, 1));
	TestEqual(TEXT("LocalRefinement dependency inheritance does not manufacture a Critical movement lane"),
		LocalWorkerPriority.WorkClass, EVoxelWorkClass::Visible);
	TestEqual(TEXT("Fine-only local demand still donates its scheduling priority to its necessary coarse dependency"),
		LocalWorkerPriority.SourcePriority, 9);
	const_cast<FVoxelInterestSet&>(Module->GetCurrentInterest()).Sources.Reset();
	MovingSettings.Budget.MaxPublishGroupsPerFrame = PreviousGroupLimit;
	FixtureWorld->DestroyWorld(false);
	GEngine->DestroyWorldContext(FixtureWorld);
	return true;
}

bool FVoxelActiveGroupReplacementTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler(true);
	if (!TestTrue(TEXT("发布夹具初始化真实控制身份与统计账本"), Scheduler.InitializeStatistics(1))) return false;
	FVoxelViewPublisher Publisher(*Module, Scheduler, 1);
	const TArray<FVoxelPublishGroupKey> Keys{{0, {0, 0}, 0}, {0, {1000, 0}, 0}, {0, {2000, 0}, 0}};
	int32 Completed = 0;
	bool bAttemptedReplacement = false;
	bool bAcceptedReplacement = false;
	Publisher.SetCommitCallbacks([&Completed]() { ++Completed; },
		[&](TConstArrayView<FVoxelPublishGroupKey>)
		{
			if (bAttemptedReplacement) return;
			for (const auto& Key : Keys)
			{
				if (!Publisher.IsGroupBusy(Key)) continue;
				bAttemptedReplacement = true;
				const FGuid Replacement = Publisher.BeginGroup(Key);
				bAcceptedReplacement = Replacement.IsValid() && Publisher.CommitGroup(Replacement);
				break;
			}
		});
	for (const auto& Key : Keys) Publisher.CommitGroup(Publisher.BeginGroup(Key));
	for (int32 Tick = 0; Tick < 8 && Publisher.IsBusy(); ++Tick) TickPublisherFrame(Publisher, *Module);
	TestTrue(TEXT("Fixture requests a replacement while the same key is actively publishing"), bAttemptedReplacement);
	TestTrue(TEXT("An active immutable publication accepts the next pending generation"), bAcceptedReplacement);
	TestEqual(TEXT("Final completion waits for both generations and runs once"), Completed, 1);
	TestFalse(TEXT("Buffered replacement eventually completes"), Publisher.IsBusy());
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelLocalPublishGroupTest,
	"WHFramework.Voxel.Rendering.LocalPublishGroups",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPendingConnectedReplacementTest,
	"WHFramework.Voxel.Rendering.PendingConnectedReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPendingConnectedReplacementTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler(true);
	if (!TestTrue(TEXT("发布夹具初始化真实控制身份与统计账本"), Scheduler.InitializeStatistics(1))) return false;
	FVoxelViewPublisher Publisher(*Module, Scheduler, 1);
	const FVoxelPublishGroupKey Parent{1, {0, 0}, 1}, Child{0, {0, 0}, 0};
	const FGuid ParentId = Publisher.BeginGroup(Parent), ChildId = Publisher.BeginGroup(Child);
	int32 Completed = 0;
	Publisher.SetCommitCallbacks([&Completed]() { ++Completed; }, {});
	Publisher.CommitGroup(ParentId);
	TickPublisherFrame(Publisher, *Module);
	TestTrue(TEXT("Connected pending groups remain outstanding"), Publisher.IsBusy());
	TestFalse(TEXT("Pending dependencies do not masquerade as active publication"), Publisher.HasActiveGroups());
	TestEqual(TEXT("An unstarted committed group coalesces new inputs into the same handle"), Publisher.BeginGroup(Parent), ParentId);
	Publisher.CommitGroup(ChildId);
	TickPublisherFrame(Publisher, *Module);
	TestEqual(TEXT("Child cannot publish before the refreshed parent is committed"), Completed, 0);
	Publisher.CommitGroup(ParentId);
	for (int32 Tick = 0; Tick < 4 && Publisher.IsBusy(); ++Tick) TickPublisherFrame(Publisher, *Module);
	TestEqual(TEXT("Refreshed connected ownership completes once"), Completed, 1);
	TestFalse(TEXT("Coalesced groups release the pending barrier"), Publisher.IsBusy());
	return true;
}

bool FVoxelLocalPublishGroupTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	auto Box = [](const double X0, const double Y0, const double X1, const double Y1)
	{
		return FBox(FVector(X0, Y0, 0), FVector(X1, Y1, 1));
	};
	TArray<FVoxelPublishFootprint> Footprints = {
		{{3, {0, 0}, 0}, Box(0, 0, 64, 64), false},
		{{2, {0, 0}, 0}, Box(0, 0, 32, 32), false},
		{{2, {1, 0}, 0}, Box(32, 0, 64, 32), false},
		{{3, {1, 0}, 0}, Box(64, 0, 128, 64), false},
		{{4, {0, 0}, 0}, Box(64, 0, 64, 16), true},
		{{3, {4, 0}, 0}, Box(256, 0, 320, 64), false},
		{{2, {8, 0}, 0}, FBox(ForceInit), false}
	};
	TArray<int32> Groups;
	FVoxelPublishGroupPlanner::Build(Footprints, Groups);
	TestEqual(TEXT("Every actor receives a group"), Groups.Num(), Footprints.Num());
	if (Groups.Num() != Footprints.Num()) return false;
	TestEqual(TEXT("Parent and both children commit together"), Groups[0], Groups[1]);
	TestEqual(TEXT("Sibling children share the parent handoff"), Groups[1], Groups[2]);
	TestEqual(TEXT("Transition joins both sides of a boundary"), Groups[0], Groups[3]);
	TestEqual(TEXT("Transition belongs to its seam group"), Groups[0], Groups[4]);
	TestNotEqual(TEXT("Distant terrain publishes independently"), Groups[0], Groups[5]);
	TestNotEqual(TEXT("Empty source has no spatial dependency"), Groups[0], Groups[6]);
	Footprints.RemoveAt(4);
	FVoxelPublishGroupPlanner::Build(Footprints, Groups);
	TestNotEqual(TEXT("Touching tiles without a transition remain independent"), Groups[0], Groups[3]);
	const FBox FineBounds = FVoxelPublishGroupPlanner::OwnershipBounds({0, {-1, 2}, 0}, 100.0);
	TestTrue(TEXT("Fine ownership bounds are valid"), FineBounds.IsValid != 0);
	TestEqual(TEXT("Negative fine coordinate starts at the correct world position"), FineBounds.Min.X, -1600.0);
	TestEqual(TEXT("Fine ownership width is sixteen cells"), FineBounds.Max.X, 0.0);
	const FBox SurfaceBounds = FVoxelPublishGroupPlanner::OwnershipBounds({2, {2, -1}, 1}, 50.0);
	TestEqual(TEXT("Surface level scales ownership width"), SurfaceBounds.Max.X - SurfaceBounds.Min.X, 1600.0);
	TestEqual(TEXT("Surface negative coordinate is aligned"), SurfaceBounds.Min.Y, -1600.0);
	const FBox MacroBounds = FVoxelPublishGroupPlanner::OwnershipBounds({3, {0, 0}, 0}, 100.0);
	TestEqual(TEXT("Macro L0 uses the stable sixteen-cell grid"), MacroBounds.Max.X, 1600.0);
	const FBox ProxyBounds = FVoxelPublishGroupPlanner::OwnershipBounds({1, {0, 0}, 1}, 100.0);
	TestEqual(TEXT("Volume proxy owner keeps its full footprint after a transition rebuild"),
		ProxyBounds.Max.X, 3200.0);
	TestFalse(TEXT("Unsupported representation has no ownership"),
		FVoxelPublishGroupPlanner::OwnershipBounds({9, {0, 0}, 0}, 100.0).IsValid != 0);
	TestFalse(TEXT("Invalid block size has no ownership"),
		FVoxelPublishGroupPlanner::OwnershipBounds({2, {0, 0}, 0}, 0.0).IsValid != 0);
	TArray<FVoxelPublishFootprint> AdjacentTransitions = {
		{{2, {0, 0}, 0}, Box(0, 0, 32, 32), false},
		{{2, {1, 0}, 0}, Box(32, 0, 64, 32), false},
		{{2, {2, 0}, 0}, Box(64, 0, 96, 32), false},
		{{4, {0, 0}, 0}, Box(0, 0, 0, 16), true},
		{{4, {2, 0}, 0}, Box(96, 0, 96, 16), true},
		{{4, {1, 0}, 0}, FBox(ForceInit), true}
	};
	FVoxelPublishGroupPlanner::Build(AdjacentTransitions, Groups);
	TestEqual(TEXT("Empty seam still commits with its owner"), Groups[1], Groups[5]);
	TestNotEqual(TEXT("Distant seams do not chain all neighboring tiles"), Groups[0], Groups[2]);
	TArray<FVoxelPublishFootprint> VolumeReplacement = {
		{{1, {0, 0}, 1}, FVoxelPublishGroupPlanner::OwnershipBounds({1, {0, 0}, 1}, 100.0), false},
		{{0, {0, 0}, 0}, FVoxelPublishGroupPlanner::OwnershipBounds({0, {0, 0}, 0}, 100.0), false},
		{{1, {1, 0}, 1}, FVoxelPublishGroupPlanner::OwnershipBounds({1, {1, 0}, 1}, 100.0), false}
	};
	FVoxelPublishGroupPlanner::Build(VolumeReplacement, Groups);
	TestEqual(TEXT("Rebuilt proxy and fine child publish together"), Groups[0], Groups[1]);
	TestNotEqual(TEXT("Adjacent proxy remains independent"), Groups[0], Groups[2]);
	const FVoxelPublishGroupKey TreeProxyKey{2, {0, 0}, 0};
	const FBox TreeProxyBounds = FVoxelPublishGroupPlanner::PublicationBounds(
		TreeProxyKey, 1.0, Box(0, 0, 36, 32), Box(0, 0, 35, 32), false);
	TArray<FVoxelPublishFootprint> TreeHandoff = {
		{TreeProxyKey, TreeProxyBounds, false},
		{{0, {2, 0}, 0}, FVoxelPublishGroupPlanner::OwnershipBounds({0, {2, 0}, 0}, 1.0), false},
		{{0, {5, 0}, 0}, FVoxelPublishGroupPlanner::OwnershipBounds({0, {5, 0}, 0}, 1.0), false}
	};
	FVoxelPublishGroupPlanner::Build(TreeHandoff, Groups);
	TestEqual(TEXT("Presented crown outside its tile commits with the adjacent Fine replacement"),
		Groups[0], Groups[1]);
	TestNotEqual(TEXT("A distant Fine region remains independent from the crown handoff"),
		Groups[0], Groups[2]);
	return true;
}

#endif
