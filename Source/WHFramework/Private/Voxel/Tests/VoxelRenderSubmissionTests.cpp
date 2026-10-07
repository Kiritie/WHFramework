#if WITH_DEV_AUTOMATION_TESTS

#include "Async/Async.h"
#include "Containers/StaticArray.h"
#include "HAL/Event.h"
#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelCoalescedSubmissionMailbox.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

namespace
{
	struct FVoxelMailboxTestPayload
	{
		uint64 Contributions[4] = {};
		uint64 Latest[4] = {};
		uint32 Mask = 0;
		bool bHasReceipt = false;
	};

	struct FVoxelMailboxTestOwner
	{
		uint64 Identity = 0;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		mutable TVoxelCoalescedSubmissionNode<FVoxelMailboxTestOwner, FVoxelMailboxTestPayload> Slot;
	};

	using FVoxelMailboxTestOwnerPtr = TSharedPtr<const FVoxelMailboxTestOwner, ESPMode::ThreadSafe>;
	using FVoxelTestSubmissionMailbox = TVoxelCoalescedSubmissionMailbox<FVoxelMailboxTestOwner, FVoxelMailboxTestPayload>;

	struct FVoxelMailboxContention
	{
		FSharedEventRef Entered{EEventMode::ManualReset};
		FSharedEventRef Release{EEventMode::ManualReset};
		TAtomic<bool> bFinished{false};
	};

	struct FVoxelMailboxConcurrentState
	{
		FVoxelTestSubmissionMailbox Mailbox;
		FVoxelMailboxTestOwnerPtr Common;
		TStaticArray<FVoxelMailboxTestOwnerPtr, 4> Owners;
		FSharedEventRef Start{EEventMode::ManualReset};
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelRenderSubmissionCoalescingTest,
	"WHFramework.Voxel.Rendering.Submission.CoalescingAndOwnerLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelRenderSubmissionCoalescingTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(7, FGuid::NewGuid(), 1, {}, 1),
		EVoxelTerrainResourcePool::Input, VoxelBoundedAllocation::SharedBytes<FVoxelMailboxTestOwner>(), Lease)) return false;
	TSharedPtr<FVoxelMailboxTestOwner, ESPMode::ThreadSafe> Owner = MakeShared<FVoxelMailboxTestOwner, ESPMode::ThreadSafe>();
	Owner->Identity = 41;
	Owner->Lease = MoveTemp(Lease);
	const TWeakPtr<FVoxelMailboxTestOwner, ESPMode::ThreadSafe> WeakOwner = Owner;
	FVoxelTestSubmissionMailbox Mailbox;
	const auto Before = Resources->Snapshot();
	Mailbox.Publish(Owner, Owner->Slot, [](FVoxelMailboxTestPayload& Data)
	{
		Data.Mask |= 1u;
		Data.Latest[0] = 1;
		Data.bHasReceipt = true;
	});
	Mailbox.Publish(Owner, Owner->Slot, [](FVoxelMailboxTestPayload& Data)
	{
		Data.Mask |= 2u;
		Data.Latest[1] = 7;
	});
	Mailbox.Publish(Owner, Owner->Slot, [](FVoxelMailboxTestPayload& Data) { Data.Latest[0] = 9; });
	Owner.Reset();
	TestTrue(TEXT("A queued submission retains its frame after the producer releases it"), WeakOwner.IsValid());
	TestEqual(TEXT("Repeated publication uses the owner's existing grant"), Resources->Snapshot().LeaseCount, Before.LeaseCount);
	TestEqual(TEXT("Coalescing does not add pool bytes"), Resources->Snapshot().Used[0], Before.Used[0]);
	FVoxelMailboxTestOwnerPtr ReceiptOwner;
	FVoxelMailboxTestPayload Payload;
	TestTrue(TEXT("One fixed slot yields one receipt"), Mailbox.TryConsume(ReceiptOwner, Payload));
	TestTrue(TEXT("Coalesced slot leaves the mailbox empty"), Mailbox.IsEmpty());
	if (!ReceiptOwner) return false;
	TestEqual(TEXT("Complete immutable owner identity survives the queue"), ReceiptOwner->Identity, 41ull);
	TestEqual(TEXT("All participating lanes remain represented"), Payload.Mask, 3u);
	TestEqual(TEXT("Repeated lane publication retains its latest completion marker"), Payload.Latest[0], 9ull);
	TestEqual(TEXT("Other lane completion marker is preserved"), Payload.Latest[1], 7ull);
	TestTrue(TEXT("The first receipt flag survives later view publications"), Payload.bHasReceipt);
	Mailbox.Publish(ReceiptOwner, ReceiptOwner->Slot, [](FVoxelMailboxTestPayload& Data)
	{
		Data.Mask = 4u;
		Data.Latest[2] = 11;
	});
	ReceiptOwner.Reset();
	TestTrue(TEXT("A consumed slot can be republished while a consumer still held its frame"), WeakOwner.IsValid());
	TestTrue(TEXT("The reused slot yields a fresh payload"), Mailbox.TryConsume(ReceiptOwner, Payload));
	TestEqual(TEXT("Requeue does not retain the previous lanes"), Payload.Mask, 4u);
	TestEqual(TEXT("Requeue clears the old first-receipt flag"), Payload.bHasReceipt, false);
	TestEqual(TEXT("Requeue clears previous completion markers"), Payload.Latest[0], 0ull);
	ReceiptOwner.Reset();
	TestFalse(TEXT("Consuming the final slot breaks its self-retention"), WeakOwner.IsValid());
	const auto After = Resources->Snapshot();
	TestEqual(TEXT("The owner releases its reservation exactly once"), After.LeaseCount, 0);
	TestEqual(TEXT("The last owner releases all lease handles"), After.LeaseHandleBytes, 0ull);
	TestEqual(TEXT("Released payload leaves only the retained ledger storage"), After.Used[0], After.LedgerMetadataBytes);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelRenderSubmissionContentionTest,
	"WHFramework.Voxel.Rendering.Submission.DetachedBatchContentionAndRequeue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelRenderSubmissionContentionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelTestSubmissionMailbox Mailbox;
	auto First = MakeShared<FVoxelMailboxTestOwner, ESPMode::ThreadSafe>();
	auto Second = MakeShared<FVoxelMailboxTestOwner, ESPMode::ThreadSafe>();
	auto Third = MakeShared<FVoxelMailboxTestOwner, ESPMode::ThreadSafe>();
	First->Identity = 1;
	Second->Identity = 2;
	Third->Identity = 3;
	Mailbox.Publish(First, First->Slot, [](FVoxelMailboxTestPayload& Data) { Data.Latest[0] = 1; });
	Mailbox.Publish(Second, Second->Slot, [](FVoxelMailboxTestPayload& Data) { Data.Latest[0] = 2; });
	FVoxelMailboxTestOwnerPtr ReceiptOwner;
	FVoxelMailboxTestPayload Payload;
	TestTrue(TEXT("Consumer detaches one producer batch"), Mailbox.TryConsume(ReceiptOwner, Payload));
	if (!ReceiptOwner) return false;
	TestEqual(TEXT("Detached stack starts at its most recent frame"), ReceiptOwner->Identity, 2ull);
	ReceiptOwner.Reset();
	Mailbox.Publish(Second, Second->Slot, [](FVoxelMailboxTestPayload& Data) { Data.Latest[0] = 22; });
	Mailbox.Publish(Third, Third->Slot, [](FVoxelMailboxTestPayload& Data) { Data.Latest[0] = 3; });
	const auto Contention = MakeShared<FVoxelMailboxContention, ESPMode::ThreadSafe>();
	TFuture<bool> Producer = Async(EAsyncExecution::Thread, [First, Contention]
	{
		FScopeLock Lock(&First->Slot.Mutex);
		Contention->Entered->Trigger();
		const bool bReleased = Contention->Release->Wait(5000);
		Contention->bFinished.Store(true);
		return bReleased;
	});
	TestTrue(TEXT("Producer owns the current detached node"), Contention->Entered->Wait(5000));
	const bool bConsumedWhileLocked = Mailbox.TryConsume(ReceiptOwner, Payload);
	TestFalse(TEXT("GameThread yields instead of waiting on a producer mutex"), bConsumedWhileLocked);
	TestFalse(TEXT("TryConsume returns while the producer still retains the lock"), Contention->bFinished.Load());
	TestFalse(TEXT("A contended detached batch is not reported as empty"), Mailbox.IsEmpty());
	Contention->Release->Trigger();
	TestTrue(TEXT("The producer leaves its bounded lock scope"), Producer.Get());
	uint64 Identities[3] = {};
	uint64 Markers[3] = {};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (!Mailbox.TryConsume(ReceiptOwner, Payload)) break;
		Identities[Index] = ReceiptOwner->Identity;
		Markers[Index] = Payload.Latest[0];
		ReceiptOwner.Reset();
	}
	TestEqual(TEXT("Consumer resumes the original detached node"), Identities[0], 1ull);
	TestEqual(TEXT("New producer batch is consumed afterward"), Identities[1], 3ull);
	TestEqual(TEXT("Requeued frame is not lost across head exchange"), Identities[2], 2ull);
	TestEqual(TEXT("Requeued frame carries its new publication"), Markers[2], 22ull);
	TestTrue(TEXT("Both detached and newly published batches are drained"), Mailbox.IsEmpty());
	while (Mailbox.TryConsume(ReceiptOwner, Payload)) ReceiptOwner.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelRenderSubmissionConcurrentTest,
	"WHFramework.Voxel.Rendering.Submission.ConcurrentProducersAndConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelRenderSubmissionConcurrentTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto State = MakeShared<FVoxelMailboxConcurrentState, ESPMode::ThreadSafe>();
	auto Common = MakeShared<FVoxelMailboxTestOwner, ESPMode::ThreadSafe>();
	Common->Identity = 0;
	State->Common = Common;
	TStaticArray<TFuture<void>, 4> Producers;
	for (int32 ProducerIndex = 0; ProducerIndex < 4; ++ProducerIndex)
	{
		auto Owner = MakeShared<FVoxelMailboxTestOwner, ESPMode::ThreadSafe>();
		Owner->Identity = uint64(ProducerIndex + 1);
		State->Owners[ProducerIndex] = Owner;
		Producers[ProducerIndex] = Async(EAsyncExecution::Thread, [State, ProducerIndex]
		{
			State->Start->Wait();
			for (uint64 Sequence = 1; Sequence <= 1000; ++Sequence)
			{
				const auto Update = [ProducerIndex, Sequence](FVoxelMailboxTestPayload& Data)
				{
					++Data.Contributions[ProducerIndex];
					Data.Latest[ProducerIndex] = Sequence;
					Data.Mask |= 1u << ProducerIndex;
				};
				State->Mailbox.Publish(State->Common, State->Common->Slot, Update);
				State->Mailbox.Publish(State->Owners[ProducerIndex], State->Owners[ProducerIndex]->Slot, Update);
			}
		});
	}
	State->Start->Trigger();
	uint64 Totals[5][4] = {};
	uint64 Latest[5][4] = {};
	const auto Consume = [&]
	{
		FVoxelMailboxTestOwnerPtr Owner;
		FVoxelMailboxTestPayload Payload;
		if (!State->Mailbox.TryConsume(Owner, Payload)) return false;
		check(Owner->Identity < 5);
		for (int32 Lane = 0; Lane < 4; ++Lane)
		{
			Totals[Owner->Identity][Lane] += Payload.Contributions[Lane];
			Latest[Owner->Identity][Lane] = FMath::Max(Latest[Owner->Identity][Lane], Payload.Latest[Lane]);
		}
		return true;
	};
	const double Deadline = FPlatformTime::Seconds() + 5.0;
	for (;;)
	{
		bool bReady = true;
		for (const auto& Producer : Producers) bReady &= Producer.IsReady();
		if (bReady || FPlatformTime::Seconds() >= Deadline) break;
		Consume();
	}
	for (auto& Producer : Producers) Producer.Get();
	while (Consume()) {}
	for (int32 Lane = 0; Lane < 4; ++Lane)
	{
		TestEqual(TEXT("Concurrent updates to a shared frame are neither lost nor duplicated"), Totals[0][Lane], 1000ull);
		TestEqual(TEXT("Each independent frame survives MPSC head exchanges"), Totals[Lane + 1][Lane], 1000ull);
		TestEqual(TEXT("Shared frame retains the latest lane marker"), Latest[0][Lane], 1000ull);
	}
	TestTrue(TEXT("Producer completion and final consumption leave no queued ownership"), State->Mailbox.IsEmpty());
	return true;
}

#endif
