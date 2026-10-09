#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Voxel/Collision/VoxelCollisionBodyIndex.h"
#include "Voxel/Task/VoxelTerrainResources.h"

namespace
{
	struct FCollisionIndexRecord
	{
		FIntVector Section = FIntVector::ZeroValue;
		FCollisionIndexRecord* Previous = nullptr;
		FCollisionIndexRecord* Next = nullptr;
		uint64 Version = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCollisionBodyIndexLifetimeTest,
	"WHFramework.Voxel.FineRefactor.Physics.BodyIndexCapacityAndCursor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCollisionBodyIndexLifetimeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	if (!Resources->Configure({})) return false;
	const FGuid Owner = FGuid::NewGuid();
	TVoxelCollisionBodyIndex<FCollisionIndexRecord> Index;
	const FIntVector Keys[] =
	{
		FIntVector(MIN_int32, MAX_int32, 0),
		FIntVector(MAX_int32, MIN_int32, -1),
		FIntVector(-1, -1, -1),
		FIntVector(-1, -1, 0),
		FIntVector(0, 0, 0)
	};
	uint64 Serial = 0;
	for (const auto& Key : Keys)
	{
		bool bAdded = false;
		auto* Record = Index.FindOrAdd(Key, *Resources,
			FVoxelGenerationConsumerKey::Section(7, Owner, 11, Key, ++Serial), bAdded);
		if (!TestTrue(TEXT("负坐标、边界坐标和共享前缀都得到独立预授予记录"), Record && bAdded)) return false;
		Record->Version = Serial;
	}
	TestEqual(TEXT("记录数量与实际接纳一致"), Index.Num(), 5);
	for (int32 KeyIndex = 0; KeyIndex < 5; ++KeyIndex)
	{
		const auto* Record = Index.Find(Keys[KeyIndex]);
		if (!TestNotNull(TEXT("固定路径完整区分三轴32位身份"), Record)) return false;
		TestEqual(TEXT("坐标不同不会覆盖既有版本"), Record->Version, uint64(KeyIndex + 1));
	}

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	const auto State = Resources->Snapshot();
	if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(7, Owner, 11, {}, ++Serial),
		EVoxelTerrainResourcePool::Resident,
		State.Limits.ResidentBytes - State.Limits.OverlapBytes - State.Used[int32(EVoxelTerrainResourcePool::Resident)], Blocker)) return false;
	bool bAdded = true;
	auto* Existing = Index.FindOrAdd(Keys[2], *Resources,
		FVoxelGenerationConsumerKey::Section(7, Owner, 11, Keys[2], ++Serial), bAdded);
	TestTrue(TEXT("容量已满时保留既有记录且不重新授予"), Existing && !bAdded && Existing->Version == 3);
	const FIntVector Missing(2, 3, 4);
	auto* Rejected = Index.FindOrAdd(Missing, *Resources,
		FVoxelGenerationConsumerKey::Section(7, Owner, 11, Missing, ++Serial), bAdded);
	TestTrue(TEXT("容量不足原子拒绝新记录"), !Rejected && !bAdded && !Index.Find(Missing));
	TestEqual(TEXT("失败接纳不改变记录数量"), Index.Num(), 5);
	Blocker.Reset();
	if (!Index.FindOrAdd(Missing, *Resources,
		FVoxelGenerationConsumerKey::Section(7, Owner, 11, Missing, ++Serial), bAdded)) return false;
	TestTrue(TEXT("实际释放容量后同一键可续行"), bAdded);

	// 删除正在轮转的相邻记录后，下一轮必须跳过已退休记录并完整访问剩余集合。
	auto* First = Index.NextForScan();
	if (!TestNotNull(TEXT("轮转具有有效起点"), First)) return false;
	auto* CursorVictim = First->Next ? First->Next : Index.GetHead();
	const FIntVector RemovedKey = CursorVictim->Section;
	Index.Remove(*CursorVictim);
	TestNull(TEXT("从下一轮游标移除的记录立即退出查询"), Index.Find(RemovedKey));
	TSet<FIntVector> Seen;
	for (int32 Scan = 0; Scan < Index.Num(); ++Scan)
	{
		auto* Record = Index.NextForScan();
		if (!TestNotNull(TEXT("退休后游标不访问已释放指针"), Record)) return false;
		Seen.Add(Record->Section);
	}
	TestEqual(TEXT("有限一轮无重复且不遗漏仍驻留的记录"), Seen.Num(), Index.Num());
	while (auto* Record = Index.GetHead()) Index.Remove(*Record);
	TestEqual(TEXT("增量退休后没有残留控制记录"), Index.Num(), 0);
	TestNull(TEXT("空索引的轮转立即结束"), Index.NextForScan());
	for (const auto& Key : Keys) TestNull(TEXT("空索引没有旧空间键"), Index.Find(Key));
	Index.Reset();
	return true;
}
#endif
