#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "EngineGlobals.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/Streaming/VoxelResidencyManager.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

namespace
{
	struct FDemandEvents final : FVoxelResidencyDemandEvents
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		FVoxelInterestDelta Delta;
		virtual const FVoxelInterestDelta& GetResidencyDelta() const override
		{
			return Delta;
		}
	};

	TSharedPtr<FDemandEvents, ESPMode::ThreadSafe> MakeDemandEvents(FVoxelWorldRuntime& InRuntime, const int32 InCount)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FDemandEvents>() +
			2 * VoxelBoundedAllocation::SetBytes<FIntVector>(InCount);
		if (!InRuntime.GetTerrainResources()->Acquire(FVoxelGenerationConsumerKey::Section(InRuntime.Epoch(),
			FGuid::NewGuid(), 1, {}, 1), EVoxelTerrainResourcePool::Input, Bytes, Lease)) return nullptr;
		auto Events = VoxelBoundedAllocation::MakeWorkerShared<FDemandEvents>(Lease);
		Events->Lease = MoveTemp(Lease);
		Events->Delta.AddedExact.Reserve(InCount);
		Events->Delta.RemovedExact.Reserve(InCount);
		return Events;
	}

	void AdvanceResidency(FVoxelResidencyManager& InManager,
		const TMap<FIntVector, FVoxelExactDemand>& InDemand = {})
	{
		for (int32 Step = 0; Step < 16; ++Step) InManager.Tick(InDemand, 1, 0.0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelResidencyDemandQueueTest,
	"WHFramework.Voxel.Streaming.Retirement.FrozenDemandQueueBudgetAndOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResidencyDemandQueueTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldRuntime Runtime(4, false, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	FVoxelTaskScheduler Scheduler;
	int32 Evictions = 0;
	FVoxelResidencyManager Manager(Runtime, Scheduler, [&Evictions](const FIntVector&) { ++Evictions; });
	Manager.SetEvictGraceFrames(0);
	TMap<FIntVector, FVoxelExactDemand> Demand;
	for (int32 Index = 0; Index < 96; ++Index)
	{
		const FIntVector Key(Index, 0, 0);
		if (!Runtime.FindOrAllocate(Key, GFrameCounter)) return false;
		Demand.Add(Key, {});
	}
	AdvanceResidency(Manager, Demand);
	auto Removed = MakeDemandEvents(Runtime, 96);
	auto Added = MakeDemandEvents(Runtime, 96);
	if (!Removed || !Added) return false;
	for (const auto& Pair : Demand)
	{
		Removed->Delta.RemovedExact.Add(Pair.Key);
		Added->Delta.AddedExact.Add(Pair.Key);
	}
	const TWeakPtr<FDemandEvents, ESPMode::ThreadSafe> RemovedReader = Removed;
	const TWeakPtr<FDemandEvents, ESPMode::ThreadSafe> AddedReader = Added;
	const uint64 InputBefore = Runtime.GetTerrainResources()->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)];
	Manager.QueueDemandEvents(Removed);
	Manager.QueueDemandEvents(Added);
	Removed.Reset();
	Added.Reset();
	TestEqual(TEXT("Queue registration performs no additional allocation or resource grant"),
		Runtime.GetTerrainResources()->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)], InputBefore);
	TestTrue(TEXT("Manager retains immutable event outputs after their producer releases them"), RemovedReader.IsValid() && AddedReader.IsValid());
	Manager.Tick(Demand, 1, 0.0);
	int32 Touched = 0;
	for (const auto& Pair : Demand) Touched += Runtime.FindSection(Pair.Key)->Retirement.Serial != 2;
	TestTrue(TEXT("A single ordinary advance consumes at most 32 delta entries"), Touched <= 32);
	TestTrue(TEXT("A large delta remains retained across a bounded yield"), RemovedReader.IsValid());
	AdvanceResidency(Manager, Demand);
	TestFalse(TEXT("First frozen event root is released after its last entry"), RemovedReader.IsValid());
	TestFalse(TEXT("Second frozen event root is consumed without replacing or losing the first"), AddedReader.IsValid());
	TestEqual(TEXT("Superseded removals cannot evict a currently demanded resource"), Evictions, 0);
	TestEqual(TEXT("All shared current resources survive delayed event processing"), Runtime.NumSections(), 96);

	const FIntVector Target(13, 0, 0);
	auto FinalRemoval = MakeDemandEvents(Runtime, 1);
	if (!FinalRemoval) return false;
	FinalRemoval->Delta.RemovedExact.Add(Target);
	Manager.QueueDemandEvents(FinalRemoval);
	FinalRemoval.Reset();
	Demand.Remove(Target);
	AdvanceResidency(Manager, Demand);
	TestNull(TEXT("The later actual removal reaches the correct current ticket"), Runtime.FindSection(Target));
	TestEqual(TEXT("Unrelated resources retain their leases"), Runtime.NumSections(), 95);
	TestEqual(TEXT("The final local removal detaches exactly once"), Evictions, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelResidencyEventWakeTest,
	"WHFramework.Voxel.Streaming.Retirement.EventWakeAndRevivalIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResidencyEventWakeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldRuntime Runtime(4, false, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	FVoxelTaskScheduler Scheduler;
	bool bReplacementPending = true;
	int32 RetentionQueries = 0;
	int32 Evictions = 0;
	FVoxelResidencyManager Manager(Runtime, Scheduler, [&Evictions](const FIntVector&) { ++Evictions; },
		[&](const FIntVector&) { ++RetentionQueries; return bReplacementPending; });
	Manager.SetEvictGraceFrames(0);
	const FIntVector Key(2, -1, 3);
	auto* Section = Runtime.FindOrAllocate(Key, GFrameCounter);
	if (!TestNotNull(TEXT("Allocation emits a candidate event"), Section)) return false;
	const auto OriginalStamp = Section->Stamp;
	const auto OriginalIdentity = Section->GetResourceIdentity();
	const uint64 OriginalSerial = Section->Retirement.Serial;
	AdvanceResidency(Manager);
	TestEqual(TEXT("Waiting ticket checks replacement once"), RetentionQueries, 1);
	TestTrue(TEXT("Actual owner records the replacement blocker"),
		EnumHasAnyFlags(Section->Retirement.Blockers, EVoxelRuntimeRetirementBlocker::Replacement));
	bReplacementPending = false;
	AdvanceResidency(Manager);
	TestNotNull(TEXT("No event means no periodic retry of sleeping candidates"), Runtime.FindSection(Key));
	TestEqual(TEXT("Repeated Tick does not scan sleeping residents"), RetentionQueries, 1);

	TMap<FIntVector, FVoxelExactDemand> Demand;
	Demand.Add(Key, {});
	Manager.NotifyDemandAdded(Key);
	TestFalse(TEXT("Revival invalidates an old ticket even with the same Section stamp"),
		Manager.WakeRetirement(Key, OriginalStamp, OriginalSerial));
	AdvanceResidency(Manager, Demand);
	TestEqual(TEXT("Revival retains the existing resource incarnation"), Section->GetResourceIdentity(), OriginalIdentity);
	Manager.RequestRetirement(Key);
	const uint64 NewSerial = Section->Retirement.Serial;
	TestTrue(TEXT("Removed lease creates a new retirement serial"), NewSerial > OriginalSerial);
	TestTrue(TEXT("Current event is accepted"), Manager.WakeRetirement(Key, OriginalStamp, NewSerial));
	TestTrue(TEXT("Repeated current event is idempotently coalesced"), Manager.WakeRetirement(Key, OriginalStamp, NewSerial));
	AdvanceResidency(Manager);
	TestNull(TEXT("Valid replacement and removed demand detach the Section"), Runtime.FindSection(Key));
	TestEqual(TEXT("Duplicate events detach once"), Evictions, 1);

	Section = Runtime.FindOrAllocate(Key, GFrameCounter);
	if (!TestNotNull(TEXT("New demand after detach creates a new instance"), Section)) return false;
	TestTrue(TEXT("Reallocated coordinate has a different incarnation"), Section->GetResourceIdentity() != OriginalIdentity);
	TestFalse(TEXT("Old section event cannot affect a new incarnation"), Manager.WakeRetirement(Key, OriginalStamp, NewSerial));
	TestFalse(TEXT("Old pin release cannot decrement a new incarnation"), Runtime.ReleaseSectionPin(Key, OriginalIdentity));
	TestTrue(TEXT("A new pin is granted by the current owner"), Runtime.TryPinSection(Key, Section->GetResourceIdentity()));
	AdvanceResidency(Manager);
	TestNotNull(TEXT("New instance remains protected"), Runtime.FindSection(Key));
	TestEqual(TEXT("Old events did not detach the new instance"), Evictions, 1);
	TestTrue(TEXT("Current last pin release wakes retirement"), Runtime.ReleaseSectionPin(Key, Section->GetResourceIdentity()));
	AdvanceResidency(Manager);
	TestNull(TEXT("New incarnation retires through its own ticket"), Runtime.FindSection(Key));
	TestEqual(TEXT("Each incarnation detaches once"), Evictions, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelResidencyGraceHeapTest,
	"WHFramework.Voxel.Streaming.Retirement.GraceHeapRemovalAndRevival",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResidencyGraceHeapTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const uint64 PreviousFrame = GFrameCounter;
	ON_SCOPE_EXIT { GFrameCounter = PreviousFrame; };
	GFrameCounter = 100;
	FVoxelWorldRuntime Runtime(4, false, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	FVoxelTaskScheduler Scheduler;
	TArray<FIntVector> Detached;
	FVoxelResidencyManager Manager(Runtime, Scheduler, [&Detached](const FIntVector& Key) { Detached.Add(Key); });
	Manager.SetEvictGraceFrames(10);
	const FIntVector Late(0, 0, 0), Middle(1, 0, 0), Early(2, 0, 0), Removed(3, 0, 0), Revived(4, 0, 0);
	if (!Runtime.FindOrAllocate(Late, 100) || !Runtime.FindOrAllocate(Middle, 98) ||
		!Runtime.FindOrAllocate(Early, 96) || !Runtime.FindOrAllocate(Removed, 97) || !Runtime.FindOrAllocate(Revived, 95)) return false;
	AdvanceResidency(Manager);
	TestEqual(TEXT("Every grace waiter has a timer slot"),
		Runtime.FindSection(Middle)->Retirement.TimerIndex == INDEX_NONE, false);
	TestTrue(TEXT("External remove unlinks the ticket before destroying its Section"), Runtime.RemoveSection(Removed));
	GFrameCounter = 102;
	const auto RevivedIdentity = Runtime.FindSection(Revived)->GetResourceIdentity();
	TestNotNull(TEXT("New lease revives an existing timer"), Runtime.FindOrAllocate(Revived, GFrameCounter));
	Manager.RequestRetirement(Revived);
	AdvanceResidency(Manager);
	GFrameCounter = 106;
	AdvanceResidency(Manager);
	TestEqual(TEXT("Earliest live deadline is selected despite insertion order and removed heap entries"), Detached.Num(), 1);
	if (Detached.Num() != 1) return false;
	TestEqual(TEXT("The earliest actual Section detaches first"), Detached[0], Early);
	TestNotNull(TEXT("Revived timer waits for its new deadline"), Runtime.FindSection(Revived));
	GFrameCounter = 108;
	AdvanceResidency(Manager);
	TestNull(TEXT("Middle deadline is released at its due frame"), Runtime.FindSection(Middle));
	TestNotNull(TEXT("Late deadline is retained"), Runtime.FindSection(Late));
	GFrameCounter = 110;
	AdvanceResidency(Manager);
	TestNull(TEXT("Late deadline becomes runnable"), Runtime.FindSection(Late));
	TestEqual(TEXT("Revival does not change resource incarnation"), Runtime.FindSection(Revived)->GetResourceIdentity(), RevivedIdentity);
	GFrameCounter = 112;
	AdvanceResidency(Manager);
	TestNull(TEXT("Rescheduled revival finally detaches"), Runtime.FindSection(Revived));
	TestEqual(TEXT("External removal never runs a duplicate owner detach"), Detached.Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelResidencyTaskZeroTest,
	"WHFramework.Voxel.Streaming.Retirement.TaskZeroAndStablePinIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResidencyTaskZeroTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldRuntime Runtime(4, false, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	FVoxelTaskScheduler Scheduler;
	if (!Scheduler.InitializeStatistics(4)) return false;
	FVoxelResidencyManager Manager(Runtime, Scheduler, {});
	Manager.SetEvictGraceFrames(0);
	const FIntVector Key(1, 2, 3);
	auto* Section = Runtime.FindOrAllocate(Key, GFrameCounter);
	if (!Section) return false;
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::GenerateExactBase;
	Request.WorkClass = EVoxelWorkClass::ExactData;
	Request.Stamp.WorldEpoch = 4;
	Request.Stamp.Section = Key;
	Request.Stamp.Token = Section->Stamp.Token;
	Request.Stamp.OwnerId = FGuid::NewGuid();
	Request.Stamp.OwnerIncarnation = Section->Incarnation;
	Request.ReservedBytes = 64;
	Request.GetBlockReason = [] { return EVoxelTaskBlockReason::Memory; };
	Request.Execute = [](const TAtomic<bool>&) { return FVoxelTaskResult(); };
	if (!TestTrue(TEXT("Real scheduler accepts blocked section work"), Scheduler.Enqueue(MoveTemp(Request)))) return false;
	AdvanceResidency(Manager);
	TestNotNull(TEXT("Queued task keeps Section resident"), Runtime.FindSection(Key));
	TestTrue(TEXT("Task blocker is recorded"), EnumHasAnyFlags(Section->Retirement.Blockers, EVoxelRuntimeRetirementBlocker::Task));
	Scheduler.CancelSection(Key);
	TestFalse(TEXT("Real cancellation brings aggregate count to zero"), Scheduler.HasSectionTask(Key));
	AdvanceResidency(Manager);
	TestNull(TEXT("The real scheduler zero event wakes its existing ticket"), Runtime.FindSection(Key));

	Section = Runtime.FindOrAllocate(Key, GFrameCounter);
	if (!Section) return false;
	TArray<FVoxelBlockState> Blocks;
	Blocks.Init(FVoxelBlockState(), 4096);
	FString Error;
	if (!Runtime.PublishBase(Key, Section->Stamp, MoveTemp(Blocks), Error) || !Runtime.PublishFinal(Key, 0, {}, {}, Error)) return false;
	const auto Identity = Section->GetResourceIdentity();
	const auto OldStamp = Section->Stamp;
	TestTrue(TEXT("A reader pins the stable allocation identity"), Runtime.TryPinSection(Key, Identity));
	Runtime.InvalidateRemoteSection(Key);
	TestTrue(TEXT("Content invalidation advances the build attempt"), Section->Stamp != OldStamp);
	TestEqual(TEXT("Changing a build attempt preserves the allocation identity"), Section->GetResourceIdentity(), Identity);
	AdvanceResidency(Manager);
	TestNotNull(TEXT("Invalidated data remains pinned"), Runtime.FindSection(Key));
	TestTrue(TEXT("Old reader can release its actual allocation after a content token change"), Runtime.ReleaseSectionPin(Key, Identity));
	TestFalse(TEXT("Duplicate pin release is rejected"), Runtime.ReleaseSectionPin(Key, Identity));
	AdvanceResidency(Manager);
	TestNull(TEXT("Last reader release wakes the new content attempt's ticket"), Runtime.FindSection(Key));
	return true;
}

#endif
