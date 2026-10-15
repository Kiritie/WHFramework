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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSourceCapacityLargeIdentityTest,
	"WHFramework.Voxel.Generation.LargeMixedSourceIdentityUnion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSourceCapacityLargeIdentityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	struct FFixture
	{
		TArray<uint64> Objects;
		mutable int32 Enumerations = 0;
		static void Enumerate(const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit Visit)
		{
			const auto& Fixture = *static_cast<const FFixture*>(InContext);
			++Fixture.Enumerations;
			for (int32 Index = 0; Index < Fixture.Objects.Num(); ++Index)
			{
				const void* Object = &Fixture.Objects[Index];
				// 数值相同的serial和object仍是不同身份；同一身份的重复读取只计一次。
				const uint64 Serial = uint64(reinterpret_cast<UPTRINT>(Object));
				for (int32 Repeat = 0; Repeat < 3; ++Repeat)
				{
					Visit({nullptr, Serial, EVoxelTerrainResourcePool::Input, uint64(8 + Index % 31), 1});
					Visit({Object, 0, EVoxelTerrainResourcePool::Working, uint64(3 + Index % 17), 2});
				}
			}
		}
	};
	FFixture Fixture;
	Fixture.Objects.SetNumZeroed(8192);
	uint64 Input = 0, Working = 0;
	for (int32 Index = 0; Index < Fixture.Objects.Num(); ++Index)
	{
		Input += uint64(8 + Index % 31);
		Working += uint64(3 + Index % 17);
	}
	FVoxelGenerationRequiredCapacity Capacity;
	FString Error;
	if (!TestTrue(TEXT("超过固定表容量的混合来源完整计量"), FVoxelGenerationRequiredSourceScope::Measure(
		&Fixture, FFixture::Enumerate, Capacity, Error))) return false;
	TestEqual(TEXT("完整serial身份去重后字节不变"), Capacity.Bytes[int32(EVoxelTerrainResourcePool::Input)], Input);
	TestEqual(TEXT("完整object身份去重后字节不变"), Capacity.Bytes[int32(EVoxelTerrainResourcePool::Working)], Working);
	TestEqual(TEXT("数值相同但类型不同的身份分别计数"), Capacity.LeaseCount, 8192 * 3);
	TestTrue(TEXT("无额外来源的资源池保持零"), Capacity.Bytes[int32(EVoxelTerrainResourcePool::Resident)] == 0);
	TestTrue(TEXT("分区重放有界且没有逐来源全链重放"), Fixture.Enumerations < 256);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSourceCapacityOwnedPrefixTest,
	"WHFramework.Voxel.Generation.OwnedInputPrefixTransferAndRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSourceCapacityOwnedPrefixTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	struct FFixture
	{
		TArray<TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>> Prefix, Published;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Previous, OtherPool;
		uint64 Bytes = 0;
		int32 Count = 0;
		static void Enumerate(const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit, bool bAggregate)
		{
			const auto& Fixture = *static_cast<const FFixture*>(Context);
			FVoxelGenerationRequiredSourceScope::VisitLease(Fixture.Previous.Get(), Visit);
			FVoxelGenerationRequiredSourceScope::VisitLease(Fixture.OtherPool.Get(), Visit);
			for (const auto& Lease : Fixture.Published) FVoxelGenerationRequiredSourceScope::VisitLease(Lease.Get(), Visit);
			if (bAggregate)
			{
				if (Fixture.Count)
				{
					Visit({&Fixture.Prefix, 0, EVoxelTerrainResourcePool::Input, Fixture.Bytes, Fixture.Count});
					Visit({&Fixture.Prefix, 0, EVoxelTerrainResourcePool::Input, Fixture.Bytes, Fixture.Count});
				}
			}
			else for (const auto& Lease : Fixture.Prefix)
				FVoxelGenerationRequiredSourceScope::VisitLease(Lease.Get(), Visit);
		}
		static void Aggregate(const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit) { Enumerate(Context, Visit, true); }
		static void Exact(const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit) { Enumerate(Context, Visit, false); }
	};
	const auto Consumer = FVoxelGenerationConsumerKey::Section(99, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!TestTrue(TEXT("绑定实际资源账本"), FVoxelTerrainResources::Create({}, Consumer, Resources, Error))) return false;
	const auto Before = Resources->Snapshot();
	FFixture Fixture;
	if (!TestTrue(TEXT("旧Input与其它池来源先授额"),
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, 256, Fixture.Previous) &&
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, 128, Fixture.OtherPool))) return false;
	const auto Compare = [&]()
	{
		FVoxelGenerationRequiredCapacity Aggregate, Exact;
		if (!TestTrue(TEXT("聚合与逐租约均能完成计量"),
			FVoxelGenerationRequiredSourceScope::Measure(&Fixture, FFixture::Aggregate, Aggregate, Error) &&
			FVoxelGenerationRequiredSourceScope::Measure(&Fixture, FFixture::Exact, Exact, Error))) return false;
		bool bEqual = Aggregate.LeaseCount == Exact.LeaseCount;
		for (int32 Pool = 0; Pool < int32(EVoxelTerrainResourcePool::Count); ++Pool)
			bEqual &= Aggregate.Bytes[Pool] == Exact.Bytes[Pool];
		return TestTrue(TEXT("原来源、全部池字节与原租约数精确一致"), bEqual);
	};
	for (int32 Index = 0; Index < 600; ++Index)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!TestTrue(TEXT("每个前缀元素来自实际独立授额"),
			Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, 64 + Index % 17, Lease))) return false;
		EVoxelTerrainResourcePool Pool;
		uint64 Bytes = 0;
		if (!Lease->ReadCapacity(Pool, Bytes) || Pool != EVoxelTerrainResourcePool::Input) return false;
		Fixture.Bytes += Bytes;
		++Fixture.Count;
		Fixture.Prefix.Add(MoveTemp(Lease));
		if (Index % 31 == 0 && !Compare()) return false;
	}
	if (!Compare()) return false;
	for (int32 Index = 0; Index < 300; ++Index)
	{
		Fixture.Bytes -= Fixture.Prefix[Index]->Bytes();
		--Fixture.Count;
		Fixture.Published.Add(MoveTemp(Fixture.Prefix[Index]));
		if (Index % 29 == 0 && !Compare()) return false;
	}
	if (!Compare()) return false;
	Fixture.Prefix.Empty();
	Fixture.Bytes = 0;
	Fixture.Count = 0;
	if (!Compare()) return false;
	Fixture.Published.Empty();
	Fixture.Previous.Reset();
	Fixture.OtherPool.Reset();
	const auto After = Resources->Snapshot();
	for (int32 Pool = 0; Pool < int32(EVoxelTerrainResourcePool::Count); ++Pool)
		TestEqual(TEXT("取消未提交前缀及释放已移交来源后全部池归还"), After.Used[Pool], Before.Used[Pool]);
	return true;
}

#endif
