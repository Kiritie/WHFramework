#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/VoxelModule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFinePresentationCapacityTest,
	"WHFramework.Voxel.FineRefactor.PresentationStagedCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFinePresentationCapacityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	Module->Epoch = 39;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(39, FGuid::NewGuid(), 1, {}, 1);
	FVoxelTerrainResourceLimits Limits;
	Limits.StagedBytes = 1024 * 1024;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources, Facts;
	TUniquePtr<FVoxelTaskScheduler> Scheduler;
	FString Error;
	if (!TestTrue(TEXT("使用原生产资源池与 Scheduler"),
		FVoxelTerrainResources::Create(Limits, Consumer, Resources, Error) &&
		FVoxelTerrainResources::Create({}, Consumer, Facts, Error) &&
		FVoxelTaskScheduler::Create(Resources.ToSharedRef(), Facts.ToSharedRef(), Consumer, Scheduler, Error))) return false;
	FVoxelViewManager Manager(*Module, *Scheduler, 39);
	const FVoxelFineShardKey Key{{}, 32};
	auto Interest = MakeShared<FVoxelInterestSet, ESPMode::ThreadSafe>();
	auto& Demand = Interest->FineShardDemands.Add(Key);
	Demand.ServicePriority.Band = EVoxelTerrainServiceBand::P2;
	Demand.ServicePriority.DistanceCells = 0.0;
	Manager.AppliedInterestSnapshot = Interest;
	FVoxelViewManager::FFineShardSlot Slot;
	FVoxelFineMeshInput Input;
	Input.WorldEpoch = 39;
	Input.Incarnation = 1;
	Input.GeneratorIdentity = 1;
	Input.Shard = Key;
	if (!TestTrue(TEXT("原 Fine 输入建立合法尝试"), Slot.Build.Initialize(Input, Error))) return false;
	const auto Attempt = Slot.Build.BeginAttempt();
	Slot.Consumer = Consumer;
	Slot.OrderIndex = Manager.FineShardSlotOrder.Add(Key);
	Slot.FirstQueuedSeconds = FPlatformTime::Seconds();
	auto Geometry = MakeShared<FVoxelFineShardGeometry, ESPMode::ThreadSafe>();
	Geometry->Attempt = Attempt;
	Geometry->GeometryIdentity = FGuid::NewGuid();
	Geometry->bCoverageValidated = Geometry->bKnownEmpty = true;
	auto Boundary = MakeShared<FVoxelFineBoundarySnapshot, ESPMode::ThreadSafe>();
	Boundary->Input = Input;
	for (int32 Face = 0; Face < 6; ++Face) { Boundary->Owned[Face].Init(0, 1); Boundary->Halo[Face].Init(0, 1); }
	for (int32 Index = 0; Index < 8; ++Index)
	{
		auto& Atom = Boundary->Atoms.AddDefaulted_GetRef();
		Atom.Section = FIntVector(Index % 2, (Index / 2) % 2, Index / 4);
		for (int32 Face = 0; Face < 6; ++Face) { Atom.Owned[Face].Init(0, 1); Atom.Halo[Face].Init(0, 1); }
	}
	auto Mesh = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	if (!TestTrue(TEXT("完整空网格证明通过原验证"), Mesh->ValidateAndCache())) return false;
	if (!TestTrue(TEXT("完成几何、边界和网格持有真实 Staged 租约"),
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, 64 * 1024, Geometry->MetadataLease) &&
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, 64 * 1024, Boundary->ResourceLease) &&
		Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, 64 * 1024, Mesh->ResourceLease))) return false;
	const uint64 OriginalGeometryLease = Geometry->MetadataLease->Serial();
	Geometry->Boundary = Boundary;
	Geometry->Pieces.Add({Key, Mesh});
	if (!TestEqual(TEXT("几何只依据实际完成结果就绪"), Slot.Build.Complete(Attempt, false, Geometry, {}),
		EVoxelFineAttemptDisposition::Accepted)) return false;
	Manager.FineShardSlots.Add(Key, MoveTemp(Slot));
	Manager.FineShardOwnershipAtomCapacity += 8;
	Manager.EnqueueFinePresentation(Key);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	const auto Before = Resources->Snapshot();
	if (!TestTrue(TEXT("真实占满 Staged，仍保留 Resident 空间"), Resources->Acquire(Consumer,
		EVoxelTerrainResourcePool::Staged, Limits.StagedBytes - Before.Used[int32(EVoxelTerrainResourcePool::Staged)], Blocker))) return false;
	TestEqual(TEXT("发布开始时 Staged 确实已满"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Staged)], Limits.StagedBytes);
	for (uint64 Frame = 1; Frame <= 16; ++Frame)
	{
		if (!Module->GetTerrainFrameContext().Begin(39, Frame, FPlatformTime::Seconds(), {})) return false;
		Manager.PrepareFineShardPresentations();
		Module->GetTerrainFrameContext().End(FPlatformTime::Seconds());
		if (!Manager.FineShardSlots.FindChecked(Key).RenderAtoms.IsEmpty()) break;
	}
	const auto& Prepared = Manager.FineShardSlots.FindChecked(Key);
	TestEqual(TEXT("满额时先迁移原几何正文"), Geometry->MetadataLease->GetPool(), EVoxelTerrainResourcePool::Resident);
	TestEqual(TEXT("边界进入同一原 Resident 池"), Boundary->ResourceLease->GetPool(), EVoxelTerrainResourcePool::Resident);
	TestEqual(TEXT("网格进入同一原 Resident 池"), Mesh->ResourceLease->GetPool(), EVoxelTerrainResourcePool::Resident);
	TestEqual(TEXT("迁移保留原租约身份"), Geometry->MetadataLease->Serial(), OriginalGeometryLease);
	TestTrue(TEXT("释放 Staged 后真实发布 Atom 能继续创建"), !Prepared.RenderAtoms.IsEmpty());
	TestTrue(TEXT("已完成内容仍保持原版本"), Prepared.Build.GetPreparedGeometry() == Geometry && Prepared.Build.GetInput() == Input);
	TestFalse(TEXT("资源迁移和 Atom 准备不能伪造已经呈现"), Prepared.bPresentationComplete);
	Manager.Reset();
	return true;
}
#endif
