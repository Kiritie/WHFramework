#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Tasks/Task.h"
#include "Voxel/Runtime/VoxelChangeHierarchy.h"

namespace
{
	bool FillInput(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease)
	{
		if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input, 1, OutLease)) return false;
		const auto State = InResources->Snapshot();
		return OutLease->Resize(State.Limits.InputBytes - State.Used[int32(EVoxelTerrainResourcePool::Input)] + OutLease->Bytes());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelChangeNativeAdmissionTest,
	"WHFramework.Voxel.Runtime.ChangeNativeAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelChangeNativeAdmissionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Owner = FVoxelGenerationConsumerKey::Section(91, FGuid::NewGuid(), 1, {}, 1);
	const auto External = FVoxelGenerationConsumerKey::Section(91, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!TestTrue(TEXT("Create original runtime ledger"), FVoxelTerrainResources::Create({}, Owner, Resources, Error))) return false;
	FVoxelChangeIndex Index;
	Index.BindResources(Resources.ToSharedRef(), Owner);
	const FIntVector Warm(0, 0, 0);
	if (!TestTrue(TEXT("Warm modified region is pregranted"), Index.SetModified(Warm, true, Error))) return false;
	FVoxelChangeHierarchy Hierarchy;
	Hierarchy.BindResources(Resources.ToSharedRef(), Owner);
	const FVoxelFineShardKey Fine{FIntVector::ZeroValue, 16};
	if (!TestTrue(TEXT("Warm fine index is pregranted"), Hierarchy.RegisterFineShard(Fine, Error))) return false;
	uint64 Revision = 0;
	if (!TestTrue(TEXT("Warm immutable edit root"), Hierarchy.InvalidateSection(Warm, Revision, Error))) return false;
	const auto Frozen = Hierarchy.CaptureVersionSnapshot();
	FVoxelAffectedRepresentationKeys Keys;
	if (!TestTrue(TEXT("Affected output has its own original Input lease"), Hierarchy.GatherAffectedFineShards(Warm, Keys, Error))) return false;
	TestTrue(TEXT("Output owns true native set backing"), Keys.Storage.GetBytes() == VoxelGenerationAllocation::AllocatedBytes(Keys.FineShards) && Keys.Storage.GetBytes() > 0);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!TestTrue(TEXT("External owner fills remaining Input"), FillInput(Resources.ToSharedRef(), External, Blocker))) return false;
	const uint64 BeforeRelease = Resources->ReleaseVersionExcludingOwner(Owner);
	uint64 Unchanged = 12345;
	TestFalse(TEXT("Cold edit refuses its immutable native path before authority publication"),
		Hierarchy.InvalidateSection({1, 0, 0}, Unchanged, Error));
	TestEqual(TEXT("Capacity failure preserves caller revision output"), Unchanged, uint64(12345));
	TestEqual(TEXT("Authority content cut remains warm"), Hierarchy.GetContentRevision(), Revision);
	TestEqual(TEXT("Missing edit has no authority revision"), Hierarchy.GetSectionRevision({1, 0, 0}), uint64(0));
	TestEqual(TEXT("Fine content revision remains warm"), Hierarchy.GetFineShardRevision(Fine), Revision);
	TestFalse(TEXT("Replacement collection needs old plus new physical backing"), Hierarchy.GatherAffectedFineShards(Warm, Keys, Error));
	TestTrue(TEXT("Rejected collection preserves its original value and lease"), Keys.FineShards.Contains(Fine) && Keys.Storage.GetBytes() > 0);
	bool bSawDenial = false;
	for (int32 Region = 1; Region < 1024; ++Region)
	{
		const FIntVector Cold(Region * 8, 0, 0);
		if (Index.SetModified(Cold, true, Error)) continue;
		bSawDenial = true;
		TestFalse(TEXT("Rejected native map growth publishes no region bit"), Index.IsModified(Cold));
		TestTrue(TEXT("Prior region remains authoritative"), Index.IsModified(Warm));
		TestEqual(TEXT("Rejected growth is recoverable external capacity"), Index.GetLastResourceAdmission(), EVoxelTerrainResourceAdmission::CapacityBlocked);
		break;
	}
	TestTrue(TEXT("True sparse/hash growth reaches admission gate"), bSawDenial);
	TestEqual(TEXT("Own failed attempts do not manufacture external release"), Resources->ReleaseVersionExcludingOwner(Owner), BeforeRelease);
	Blocker.Reset();
	TestTrue(TEXT("Original edit retries after external release"), Hierarchy.InvalidateSection({1, 0, 0}, Unchanged, Error));
	TestTrue(TEXT("Frozen root retains its original cut"), Frozen.GetSectionRevision(Warm) == Revision && Frozen.GetSectionRevision({1, 0, 0}) == 0);
	int32 Visits = 0;
	Hierarchy.VisitAffectedFineShards(Warm, [&](const FVoxelFineShardKey& Key)
	{
		++Visits;
		// 实际回调可释放同一Hierarchy；访问回调不持Hierarchy锁。
		TestTrue(TEXT("Visitor can reenter final-ref release"), Hierarchy.ReleaseFineShard(Key, Error));
	});
	TestEqual(TEXT("Frozen visitor delivers the original record once"), Visits, 1);
	Visits = 0;
	Hierarchy.VisitAffectedFineShards(Warm, [&](const FVoxelFineShardKey&) { ++Visits; });
	TestEqual(TEXT("Reentrant release leaves no active fine reference"), Visits, 0);
	FVoxelChangeHierarchy Foreign;
	FVoxelPreparedChangeInvalidations Prepared;
	const auto Section = [](int32) { return FIntVector(3, 0, 0); };
	TestTrue(TEXT("Foreign passive fixture prepares an edit root"), Foreign.PrepareInvalidations(1, Section, Prepared, Error));
	const uint64 Before = Hierarchy.GetContentRevision();
	TestFalse(TEXT("Prepared roots cannot cross hierarchy owner"), Hierarchy.CommitInvalidations(MoveTemp(Prepared), Section, Error));
	TestEqual(TEXT("Cross-owner rejection leaves authority untouched"), Hierarchy.GetContentRevision(), Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPublishedNativeRetirementTest,
	"WHFramework.Voxel.Runtime.PublishedNativeRetirement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPublishedNativeRetirementTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Owner = FVoxelGenerationConsumerKey::Section(93, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!TestTrue(TEXT("Create original control ledger"), FVoxelTerrainResources::Create({}, Owner, Resources, Error))) return false;
	const uint64 NativeBytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(64);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ScratchLease;
	if (!TestTrue(TEXT("Candidate scratch reserves native heap before allocation"), Resources->Acquire(Owner, EVoxelTerrainResourcePool::Input, NativeBytes, ScratchLease))) return false;
	TArray<uint8> Scratch;
	Scratch.Reserve(64);
	Scratch.AddZeroed(64);
	TestEqual(TEXT("Charged bytes match actual candidate backing"), VoxelGenerationAllocation::AllocatedBytes(Scratch), NativeBytes);
	const uint64 Before = Resources->ReleaseVersionExcludingOwner(Owner);
	Scratch.Empty();
	ScratchLease.Reset();
	TestEqual(TEXT("Unpublished candidate refund does not wake its original consumer"), Resources->ReleaseVersionExcludingOwner(Owner), Before);
	TestEqual(TEXT("Candidate refund is not a published retirement"), Resources->Snapshot().PublishedRetirementVersion, uint64(0));
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> BodyLease;
	if (!TestTrue(TEXT("Published body also reserves before its real heap"), Resources->Acquire(Owner, EVoxelTerrainResourcePool::Input, NativeBytes, BodyLease))) return false;
	TArray<uint8> Body;
	Body.Reserve(64);
	Body.AddZeroed(64);
	const uint64 Serial = BodyLease->Serial();
	BodyLease->MarkPublished();
	TestTrue(TEXT("Publication marker is visible on its same serial"), BodyLease->IsPublished() && BodyLease->Serial() == Serial);
	BodyLease->MarkPublished();
	auto LastHold = BodyLease;
	Body.Empty();
	TestTrue(TEXT("Payload contraction succeeds after physical backing free"), BodyLease->Resize(0));
	TestEqual(TEXT("Resize is not a published-last-hold event"), Resources->Snapshot().PublishedRetirementVersion, uint64(0));
	BodyLease.Reset();
	TestEqual(TEXT("Earlier strong release cannot publish retirement"), Resources->ReleaseVersionExcludingOwner(Owner), Before);
	LastHold.Reset();
	const auto Retired = Resources->Snapshot();
	TestTrue(TEXT("Same-owner actual last hold wakes the original consumer"), Retired.PublishedRetirementVersion > Before);
	TestEqual(TEXT("Publication retirement uses the original release clock"), Retired.PublishedRetirementVersion, Retired.ReleaseVersion);
	TestEqual(TEXT("Blocking owner query includes published physical retirement"), Resources->ReleaseVersionExcludingOwner(Owner), Retired.PublishedRetirementVersion);
	uint64 TryVersion = 0;
	TestTrue(TEXT("Nonblocking owner query has the same result"), Resources->TryGetReleaseVersionExcludingOwner(Owner, TryVersion));
	TestEqual(TEXT("Both query APIs agree"), TryVersion, Retired.PublishedRetirementVersion);
	if (!TestTrue(TEXT("A later uncommitted attempt can reserve"), Resources->Acquire(Owner, EVoxelTerrainResourcePool::Input, NativeBytes, ScratchLease))) return false;
	Scratch.Reserve(64);
	Scratch.AddZeroed(64);
	Scratch.Empty();
	ScratchLease.Reset();
	TestEqual(TEXT("Later own refund cannot manufacture a second published wake"), Resources->ReleaseVersionExcludingOwner(Owner), Retired.PublishedRetirementVersion);
	return true;
}

namespace
{
	class FFinePinRetirementCommand final : public IAutomationLatentCommand
	{
		enum class EPhase : uint8
		{
			None = 0,
			Initialize,
			WorkerRelease,
			DetachedRelease,
			WeakRetirement,
			Retire
		};
	public:
		explicit FFinePinRetirementCommand(FAutomationTestBase& InTest)
			: Test(InTest)
		{
		}

		virtual ~FFinePinRetirementCommand()
		{
		}

		virtual bool Update() override
		{
			if (Phase == EPhase::Initialize)
			{
				Deadline = FPlatformTime::Seconds() + 8.0;
				Consumer = FVoxelGenerationConsumerKey::Section(92, FGuid::NewGuid(), 1, {}, 1);
				External = FVoxelGenerationConsumerKey::Section(92, FGuid::NewGuid(), 1, {}, 1);
				if (!FVoxelTerrainResources::Create({}, Consumer, Resources, Error)) { Test.AddError(Error); return true; }
				Hierarchy = MakeUnique<FVoxelChangeHierarchy>();
				Hierarchy->BindResources(Resources.ToSharedRef(), Consumer);
				if (!Hierarchy->PinFineShard(Fine, Token, Error)) { Test.AddError(Error); return true; }
				Weak = Token;
				if (!FillInput(Resources.ToSharedRef(), External, Blocker)) { Test.AddError(TEXT("Cannot fill original ledger")); return true; }
				auto Previous = Token;
				Test.TestFalse(TEXT("Denied node grant cannot add another fine reference"), Hierarchy->PinFineShard(Fine, Token, Error));
				Test.TestTrue(TEXT("Rejected pin preserves the old token"), Token.Get() == Previous.Get());
				Previous.Reset();
				Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [WorkerToken = MoveTemp(Token)]() mutable
				{
					WorkerToken.Reset();
				});
				Phase = EPhase::WorkerRelease;
			}
			if (Phase == EPhase::WorkerRelease && Task.IsCompleted())
			{
				Test.TestTrue(TEXT("Worker last-ref release only enqueues the original node"), Hierarchy->HasPendingFineReleases());
				// 当前visitor持有原根，使Remove必须走共享COW路径；回调本身不持Hierarchy锁。
				Hierarchy->VisitAffectedFineShards({}, [&](const FVoxelFineShardKey&)
				{
					Test.TestEqual(TEXT("Shared frozen reader and full Input preserve the pending ref"), Hierarchy->AdvanceFineReleases(1), 0);
					const auto Before = Resources->Snapshot();
					Test.TestEqual(TEXT("Own refund cannot wake the shared path"), Hierarchy->AdvanceFineReleases(1), 0);
					Test.TestEqual(TEXT("Blocked retirement does not hot-loop resource admission"), Resources->Snapshot().Version, Before.Version);
				});
				const uint64 BeforeReaderExit = Resources->ReleaseVersionExcludingOwner(Consumer);
				Test.TestEqual(TEXT("Reader exit makes the original authority path allocation-free without an external release"), Hierarchy->AdvanceFineReleases(1), 1);
				const auto AfterUnique = Resources->Snapshot();
				const uint64 AfterUniqueRelease = Resources->ReleaseVersionExcludingOwner(Consumer);
				Test.TestTrue(TEXT("Unique retirement wakes only through real published body release"),
					AfterUniqueRelease == BeforeReaderExit || AfterUnique.PublishedRetirementVersion == AfterUniqueRelease);
				Blocker.Reset();
				Test.TestFalse(TEXT("One successful retirement consumes the intrusive node"), Hierarchy->HasPendingFineReleases());
				int32 Active = 0;
				Hierarchy->VisitAffectedFineShards({}, [&](const FVoxelFineShardKey&) { ++Active; });
				Test.TestEqual(TEXT("Failed duplicate pin did not leak a reference"), Active, 0);
				TSharedPtr<const void, ESPMode::ThreadSafe> Late;
				if (!Hierarchy->PinFineShard(Fine, Late, Error)) { Test.AddError(Error); return true; }
				Hierarchy->Reset();
				if (!Hierarchy->RegisterFineShard(Fine, Error)) { Test.AddError(Error); return true; }
				Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [WorkerToken = MoveTemp(Late)]() mutable
				{
					WorkerToken.Reset();
				});
				Phase = EPhase::DetachedRelease;
			}
			if (Phase == EPhase::DetachedRelease && Task.IsCompleted())
			{
				int32 Active = 0;
				Hierarchy->VisitAffectedFineShards({}, [&](const FVoxelFineShardKey&) { ++Active; });
				Test.TestEqual(TEXT("Late old-state release cannot remove a newly registered same key"), Active, 1);
				Test.TestFalse(TEXT("Detached release cannot enqueue into the new state"), Hierarchy->HasPendingFineReleases());
				Hierarchy->Reset();
				Hierarchy.Reset();
				Task = UE::Tasks::FTask();
				Phase = EPhase::WeakRetirement;
			}
			if (Phase == EPhase::WeakRetirement)
			{
				const auto State = Resources->Snapshot();
				// Coverage在worker删除；先等其无关授额退休，再观察token的controller寿命。
				if (State.LeaseCount <= 2)
				{
					Test.TestFalse(TEXT("Dead token has no live strong pointer"), Weak.IsValid());
					Test.TestEqual(TEXT("Weak token controller and its frozen state retain exactly two original grants"), State.LeaseCount, 2);
					Test.TestTrue(TEXT("Those roots retain native Input beyond the ledger's metadata"),
						State.Used[int32(EVoxelTerrainResourcePool::Input)] > State.RootInputBytes + State.LedgerMetadataBytes);
					Weak.Reset();
					Phase = EPhase::Retire;
				}
			}
			if (Phase == EPhase::Retire && Resources->Snapshot().LeaseCount == 0) return true;
			if (FPlatformTime::Seconds() >= Deadline)
			{
				Test.AddError(TEXT("Bounded nonblocking fine pin retirement timed out"));
				Blocker.Reset();
				if (Hierarchy) Hierarchy->Reset();
				Token.Reset();
				Weak.Reset();
				return true;
			}
			return false;
		}
	private:
		FAutomationTestBase& Test;
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
		TUniquePtr<FVoxelChangeHierarchy> Hierarchy;
		FVoxelGenerationConsumerKey Consumer;
		FVoxelGenerationConsumerKey External;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
		TSharedPtr<const void, ESPMode::ThreadSafe> Token;
		TWeakPtr<const void, ESPMode::ThreadSafe> Weak;
		UE::Tasks::FTask Task;
		FVoxelFineShardKey Fine{FIntVector::ZeroValue, 16};
		FString Error;
		double Deadline = 0;
		EPhase Phase = EPhase::Initialize;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFinePinNativeRetirementTest,
	"WHFramework.Voxel.Runtime.FinePinNativeRetirement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFinePinNativeRetirementTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	ADD_LATENT_AUTOMATION_COMMAND(FFinePinRetirementCommand(*this));
	return true;
}
#endif
