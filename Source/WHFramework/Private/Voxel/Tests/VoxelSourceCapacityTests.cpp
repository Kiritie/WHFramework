#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
	struct FOriginalSourceFixture
	{
		TArray<TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>, TFixedAllocator<600>> Leases;
		int32 Count = 0;
		mutable int32 Enumerations = 0;
		FVoxelGenerationRequiredSource Extra;
		bool bExtra = false;

		static void Enumerate(const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			const auto& Fixture = *static_cast<const FOriginalSourceFixture*>(InContext);
			++Fixture.Enumerations;
			for (int32 Index = 0; Index < Fixture.Count; ++Index)
			{
				FVoxelGenerationRequiredSourceScope::VisitLease(Fixture.Leases[Index].Get(), InVisit);
				FVoxelGenerationRequiredSourceScope::VisitLease(Fixture.Leases[Index].Get(), InVisit);
			}
			if (Fixture.bExtra) InVisit(Fixture.Extra);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelSourceCapacityTest,
	"WHFramework.Voxel.Generation.OriginalSourceCapacityDeduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSourceCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(97, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!TestTrue(TEXT("Fixture binds its original native ledger"),
		FVoxelTerrainResources::Create({}, Consumer, Resources, Error))) return false;
	FOriginalSourceFixture Fixture;
	for (int32 Index = 0; Index < 600; ++Index)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!TestTrue(TEXT("Each source has an actual original Input grant"),
			Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, 64, Lease) == EVoxelTerrainResourceAdmission::Granted)) return false;
		Fixture.Leases.Add(MoveTemp(Lease));
	}
	const auto Before = Resources->Snapshot();
	FVoxelGenerationRequiredSourceScope Outer(Resources.Get(), &Fixture, FOriginalSourceFixture::Enumerate);
	FVoxelGenerationRequiredSourceScope Inner(Resources.Get(), &Fixture, FOriginalSourceFixture::Enumerate);
	Fixture.Count = 128;
	FVoxelGenerationRequiredCapacity Capacity;
	if (!TestTrue(TEXT("Repeated readers collect the same original source chain"),
		FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Capacity, Error))) return false;
	TestEqual(TEXT("Actual Input sources are counted once across both readers"), Capacity.Bytes[int32(EVoxelTerrainResourcePool::Input)], uint64(128 * 64));
	TestEqual(TEXT("Actual lease identities are counted once"), Capacity.LeaseCount, 128);
	TestEqual(TEXT("The common closure enumerates each original reader only once"), Fixture.Enumerations, 2);
	Fixture.Enumerations = 0;
	TestTrue(TEXT("Excluding an actual lease preserves the remaining original closure"),
		FVoxelGenerationRequiredSourceScope::Collect(*Resources, Fixture.Leases[7].Get(), Capacity, Error));
	TestEqual(TEXT("Excluded serial is omitted from every reader"), Capacity.LeaseCount, 127);
	TestEqual(TEXT("Excluded original bytes are omitted"), Capacity.Bytes[int32(EVoxelTerrainResourcePool::Input)], uint64(127 * 64));
	TestEqual(TEXT("Exclusion also uses one enumeration per reader"), Fixture.Enumerations, 2);
	Fixture.Count = 600;
	TestTrue(TEXT("A closure larger than the fixed stack identity table remains valid"),
		FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Capacity, Error));
	TestEqual(TEXT("Large native closure keeps all distinct leases"), Capacity.LeaseCount, 600);
	TestEqual(TEXT("Large native closure keeps all actual bytes without duplicate readers"), Capacity.Bytes[int32(EVoxelTerrainResourcePool::Input)], uint64(600 * 64));
	TestEqual(TEXT("Measurement acquires no additional original Input capacity"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)], Before.Used[int32(EVoxelTerrainResourcePool::Input)]);
	Fixture.Count = 0;
	Fixture.bExtra = true;
	Fixture.Extra = {&Fixture, 0, EVoxelTerrainResourcePool::Working, 48, 3};
	TestTrue(TEXT("Original immutable object identities use the same deduplication contract"),
		FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Capacity, Error));
	TestEqual(TEXT("Repeated aggregate object source is counted once"), Capacity.Bytes[int32(EVoxelTerrainResourcePool::Working)], uint64(48));
	TestEqual(TEXT("Aggregate source preserves its original lease count"), Capacity.LeaseCount, 3);
	const auto Previous = Capacity;
	Fixture.Extra.Object = nullptr;
	TestFalse(TEXT("Invalid original identity still fails measurement"),
		FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Capacity, Error));
	TestTrue(TEXT("Failed collection retains the previous output"),
		Capacity.LeaseCount == Previous.LeaseCount && Capacity.Bytes[int32(EVoxelTerrainResourcePool::Working)] == Previous.Bytes[int32(EVoxelTerrainResourcePool::Working)]);
	Fixture.Extra = {&Fixture, 0, EVoxelTerrainResourcePool::Input, MAX_uint64, MAX_int32};
	Fixture.Count = 1;
	TestFalse(TEXT("Original source byte and lease count overflow remains explicit"),
		FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Capacity, Error));
	TestTrue(TEXT("Overflow also retains the previous output"),
		Capacity.LeaseCount == Previous.LeaseCount && Error.StartsWith(TEXT("CapacityUnachievable")));
	return true;
}

#endif
