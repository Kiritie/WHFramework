#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Voxel/Collision/VoxelCollisionPresenter.h"
#include "Voxel/Collision/VoxelPhysicsRetirement.h"
#include "Voxel/Geometry/VoxelCollisionBuilder.h"
#include "Voxel/Navigation/VoxelNavigationTypes.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPhysicsFrozenIdentityTest,
	"WHFramework.Voxel.FineRefactor.Physics.FrozenInputIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsFrozenIdentityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelTaskStamp Input;
	Input.OwnerId = FGuid::NewGuid();
	Input.OwnerIncarnation = 11;
	Input.WorldEpoch = 17;
	Input.Token = 23;
	Input.Revision = 29;
	Input.GeometryRevision = 31;
	Input.Section = FIntVector(-5, 7, 9);
	Input.RegistryRevision = 37;
	Input.ShapeRevision = 41;
	FVoxelPhysicsReceipt Receipt;
	Receipt.WorldEpoch = Input.WorldEpoch;
	Receipt.SceneEpoch = FGuid::NewGuid();
	Receipt.PhysicsStep = 43;
	Receipt.Section = Input.Section;
	Receipt.SectionToken = Input.Token;
	Receipt.DataRevision = Input.Revision;
	Receipt.ContentRevision = Input.GeometryRevision;
	Receipt.RegistryRevision = Input.RegistryRevision;
	Receipt.ShapeRevision = Input.ShapeRevision;
	Receipt.BodyGeneration = 47;
	Receipt.SectionIncarnation = Input.OwnerIncarnation;
	Receipt.PresenterOwnerId = Input.OwnerId;
	TestTrue(TEXT("完整冻结输入匹配真实物理回执"), Receipt.MatchesInput(Input, Receipt.SceneEpoch));
	TestFalse(TEXT("旧 Scene 回执不可复用"), Receipt.MatchesInput(Input, FGuid::NewGuid()));
	for (int32 Field = 0; Field < 9; ++Field)
	{
		FVoxelTaskStamp Changed = Input;
		switch (Field)
		{
		case 0: Changed.OwnerId = FGuid::NewGuid(); break;
		case 1: ++Changed.OwnerIncarnation; break;
		case 2: ++Changed.WorldEpoch; break;
		case 3: ++Changed.Token; break;
		case 4: ++Changed.Revision; break;
		case 5: ++Changed.GeometryRevision; break;
		case 6: ++Changed.Section.Z; break;
		case 7: ++Changed.RegistryRevision; break;
		case 8: ++Changed.ShapeRevision; break;
		}
		TestFalse(TEXT("任一身份或内容版本变化都使旧回执不再 Ready"), Receipt.MatchesInput(Changed, Receipt.SceneEpoch));
		TSet<FVoxelTaskKey> Tasks;
		Tasks.Add({EVoxelTaskKind::BuildCollision, Input});
		Tasks.Add({EVoxelTaskKind::BuildCollision, Changed});
		TestEqual(TEXT("调度键保留不同冻结版本，旧任务不会吞掉新任务"), Tasks.Num(), 2);
	}
	Receipt.PhysicsStep = INDEX_NONE;
	TestFalse(TEXT("Worker 完成不能替代 PT 回执"), Receipt.MatchesInput(Input, Receipt.SceneEpoch));
	Receipt.PhysicsStep = 43;
	Receipt.BodyGeneration = 0;
	TestFalse(TEXT("未安装 Body 不提供有效回执"), Receipt.MatchesInput(Input, Receipt.SceneEpoch));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCollisionFrozenBuildTest,
	"WHFramework.Voxel.FineRefactor.Physics.BuildIdentityAndCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCollisionFrozenBuildTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelRegistrySnapshot Registry = *VoxelTest::MakeRegistry();
	Registry.Definitions[1].bSolid = true;
	FVoxelShapeRegistry Shapes;
	Shapes.BuildDefaults();
	FVoxelSectionSnapshot Snapshot;
	Snapshot.Section = FIntVector(-3, 5, 7);
	Snapshot.Stamp.Epoch = 17;
	Snapshot.Stamp.Token = 19;
	Snapshot.Revision = 23;
	Snapshot.Blocks.Init(FVoxelBlockState{1, 0}.Pack(), VoxelBlock::Volume);
	FVoxelSectionCollisionResult Result;
	Result.Stamp.OwnerId = FGuid::NewGuid();
	Result.Stamp.OwnerIncarnation = 29;
	Result.Stamp.GeometryRevision = 31;
	const FGuid Owner = Result.Stamp.OwnerId;
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionCollisionResult>() +
		VoxelBoundedAllocation::ArrayBytes<FBox>(16384);
	if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(17, Owner, 29, Snapshot.Section, 1),
		EVoxelTerrainResourcePool::Staged, Bytes, Result.ResourceLease)) return false;
	if (!TestTrue(TEXT("生产 Builder 在已授予容量内生成整个实心 Section"),
		FVoxelCollisionBuilder::Build(Snapshot, Registry, Shapes, Result))) return false;
	TestEqual(TEXT("立方体贪心合并保留为一个完整盒"), Result.Boxes.Num(), 1);
	TestTrue(TEXT("合并盒覆盖整个 Section"), Result.Boxes[0] == FBox(FVector::ZeroVector, FVector(16)));
	TestTrue(TEXT("建造期间保留所有者"), Result.Stamp.OwnerId == Owner);
	TestEqual(TEXT("资源实例不被构建 Token 覆盖"), Result.Stamp.OwnerIncarnation, uint64(29));
	TestEqual(TEXT("保留冻结内容版本"), Result.Stamp.GeometryRevision, uint64(31));
	TestEqual(TEXT("记录实际 Registry 输入"), Result.Stamp.RegistryRevision, Registry.Hash);
	TestEqual(TEXT("记录实际 Shape 输入"), Result.Stamp.ShapeRevision, Shapes.GetContentHash());
	TestTrue(TEXT("真实输出容量未超过原授予"), Result.Bytes() <= Result.ResourceLease->Bytes());
	const FVoxelTaskStamp Installed = Result.Stamp;
	++Registry.Hash;
	TestFalse(TEXT("重建峰值不足时在分配前等待"), FVoxelCollisionBuilder::Build(Snapshot, Registry, Shapes, Result));
	TestTrue(TEXT("等待保留旧输出完整身份"), Result.Stamp == Installed);
	TestEqual(TEXT("等待保留旧碰撞盒"), Result.Boxes.Num(), 1);
	if (!Result.ResourceLease->Resize(Bytes + Result.Boxes.GetAllocatedSize())) return false;
	TAtomic<bool> Canceled{true};
	TestFalse(TEXT("取消不发布候选或新版 Registry 身份"), FVoxelCollisionBuilder::Build(Snapshot, Registry, Shapes, Result, &Canceled));
	TestTrue(TEXT("取消保留旧身份"), Result.Stamp == Installed);
	TestTrue(TEXT("容量释放后允许同一输出重建"), FVoxelCollisionBuilder::Build(Snapshot, Registry, Shapes, Result));
	TestEqual(TEXT("成功只发布实际新 Registry 版本"), Result.Stamp.RegistryRevision, Registry.Hash);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNativeRetirementFenceTest,
	"WHFramework.Voxel.FineRefactor.Physics.NativeDestructionBarrier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNativeRetirementFenceTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelNativeRetirementFence Fence;
	TestFalse(TEXT("场景移除或时间流逝本身不证明原生代理销毁"), Fence.IsPastDestruction(100));
	TestTrue(TEXT("第一次原生销毁事件记录 Callback 执行序号"), Fence.ObserveDestruction(100));
	TestFalse(TEXT("销毁事件内部尚未跨过 native delete，不准退休"), Fence.IsPastDestruction(100));
	TestFalse(TEXT("较早执行序号不能形成后续销毁证明"), Fence.IsPastDestruction(99));
	TestFalse(TEXT("重复事件不重新排队或修改原屏障"), Fence.ObserveDestruction(101));
	TestTrue(TEXT("同一 Callback 后续执行已跨过 native delete"), Fence.IsPastDestruction(101));
	FVoxelNativeRetirementFence Wrap;
	Wrap.ObserveDestruction(MAX_uint64);
	TestTrue(TEXT("无符号执行序号回绕仍能识别后续屏障"), Wrap.IsPastDestruction(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNavigationPhysicsIdentityTest,
	"WHFramework.Voxel.FineRefactor.Physics.NavigationBodyIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNavigationPhysicsIdentityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelPhysicsReceipt Receipt;
	Receipt.WorldEpoch = 7;
	Receipt.SceneEpoch = FGuid::NewGuid();
	Receipt.PresenterOwnerId = FGuid::NewGuid();
	Receipt.PhysicsStep = 11;
	Receipt.Section = FIntVector(-3, 5, 9);
	Receipt.SectionIncarnation = 13;
	Receipt.SectionToken = 17;
	Receipt.DataRevision = 19;
	Receipt.ContentRevision = 23;
	Receipt.RegistryRevision = 29;
	Receipt.ShapeRevision = 31;
	Receipt.BodyGeneration = 37;
	FVoxelNavigationInputVersion Version;
	Version.WorldEpoch = Receipt.WorldEpoch;
	Version.Section = Receipt.Section;
	Version.SectionIncarnation = Receipt.SectionIncarnation;
	Version.SectionToken = Receipt.SectionToken;
	Version.SectionRevision = Receipt.DataRevision;
	Version.LocalGeometryRevision = Receipt.ContentRevision;
	Version.RegistryRevision = Receipt.RegistryRevision;
	Version.ShapeRevision = Receipt.ShapeRevision;
	Version.PhysicsSceneEpoch = Receipt.SceneEpoch;
	Version.PhysicsOwnerId = Receipt.PresenterOwnerId;
	Version.CollisionBodyGeneration = Receipt.BodyGeneration;
	Version.AboveIncarnation = 41;
	Version.AboveToken = 43;
	Version.AboveRevision = 47;
	Version.AboveGeometryRevision = 53;
	Version.CellSize = 25.0;
	Version.Clearance = 180.0;
	TestTrue(TEXT("Nav 输入必须来自当前已提交 Body"), Version.MatchesPhysicsReceipt(Receipt));
	for (int32 Field = 0; Field < 11; ++Field)
	{
		auto Changed = Receipt;
		switch (Field)
		{
		case 0: ++Changed.WorldEpoch; break;
		case 1: Changed.SceneEpoch = FGuid::NewGuid(); break;
		case 2: Changed.PresenterOwnerId = FGuid::NewGuid(); break;
		case 3: ++Changed.Section.Z; break;
		case 4: ++Changed.SectionIncarnation; break;
		case 5: ++Changed.SectionToken; break;
		case 6: ++Changed.DataRevision; break;
		case 7: ++Changed.ContentRevision; break;
		case 8: ++Changed.RegistryRevision; break;
		case 9: ++Changed.ShapeRevision; break;
		case 10: ++Changed.BodyGeneration; break;
		}
		TestFalse(TEXT("同数据重新安装、局部变化或旧世界 Body 均不能沿用旧 Nav 版本"),
			Version.MatchesPhysicsReceipt(Changed));
	}
	Receipt.PhysicsStep = INDEX_NONE;
	TestFalse(TEXT("准备完成但没有PT提交证明不能接纳 Nav"), Version.MatchesPhysicsReceipt(Receipt));
	for (int32 Field = 0; Field < 4; ++Field)
	{
		auto Changed = Version;
		switch (Field)
		{
		case 0: ++Changed.AboveIncarnation; break;
		case 1: ++Changed.AboveToken; break;
		case 2: ++Changed.AboveRevision; break;
		case 3: ++Changed.AboveGeometryRevision; break;
		}
		TestFalse(TEXT("上层净空输入的稳定身份或内容变化独立使旧 Nav 结果失效"), Changed == Version);
	}
	return true;
}
#endif
