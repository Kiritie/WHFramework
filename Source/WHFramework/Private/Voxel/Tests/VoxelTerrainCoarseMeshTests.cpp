#include "Voxel/Rendering/VoxelTerrainCoarseMesh.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Async/Async.h"
#include "Voxel/Rendering/VoxelCoarseBuildStorage.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Runtime/VoxelFineOverlayRead.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	using FGeometryPtr = TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe>;
	using FPatchPtr = TSharedPtr<const FVoxelTerrainCoarsePatch, ESPMode::ThreadSafe>;

	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> MakeSource()
	{
		auto Source = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
		Source->Stamp.WorldEpoch = 1;
		Source->Stamp.OwnerId = FGuid::NewGuid();
		Source->Stamp.OwnerIncarnation = Source->Stamp.Token = 1;
		Source->Stamp.bHasGeometryIdentity = true;
		Source->Stamp.GeometryIdentity.Key = {EVoxelTerrainRepresentation::Surface, FIntVector::ZeroValue, 20};
		Source->Stamp.GeometryIdentity.SampleStepCells = 1024;
		auto& Mesh = Source->Batches.AddDefaulted_GetRef().Mesh;
		for (int32 Triangle = 0; Triangle < 64; ++Triangle)
		{
			const FVector Origin = Triangle < 32 ? FVector::ZeroVector : FVector(1000000, 0, 0);
			for (const FVector& Vertex : {Origin, Origin + FVector(16, 0, 0), Origin + FVector(0, 16, 0)})
			{
				Mesh.Triangles.Add(Mesh.Vertices.Num());
				Mesh.Vertices.Add(Vertex);
				Mesh.Normals.Add(FVector::UpVector);
				Mesh.UV0.Add(FVector2D(Vertex.X, Vertex.Y));
				Mesh.UV1.Add(FVector2D(Vertex.X * 2, Vertex.Y * 2));
				Mesh.UV2.Add(FVector2D(Vertex.X * 3, Vertex.Y * 3));
				Mesh.Colors.Add(FLinearColor::White);
				Mesh.Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
			}
		}
		Source->ValidateAndCache();
		return Source;
	}

	FGeometryPtr BuildGeometry(const TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe>& Resources,
		const FVoxelGenerationConsumerKey& Consumer, const TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe>& Source)
	{
		FVoxelTerrainCoarseMeshBuilder Builder(Resources, Consumer);
		FString Error;
		if (!Builder.Initialize(Source, FVector::ZeroVector, 1.0, Source->Stamp, Error)) return nullptr;
		TAtomic<bool> Cancel{false};
		for (int32 Slice = 0; Slice < 1024; ++Slice)
		{
			const auto Progress = Builder.Advance(1, Cancel, Error);
			if (Progress == EVoxelTerrainCoarseMeshProgress::Complete) return Builder.TakeResult();
			if (Progress != EVoxelTerrainCoarseMeshProgress::Working) return nullptr;
		}
		return nullptr;
	}

	FPatchPtr BuildPatch(const TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe>& Resources,
		const FVoxelGenerationConsumerKey& Consumer, const FGeometryPtr& Geometry, const FPatchPtr& Base,
		TConstArrayView<FBox> Boxes, TConstArrayView<FBox> Changes)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::SharedBytes<FVoxelTerrainCoarseExclusions>() + VoxelBoundedAllocation::ArrayBytes<FBox>(Boxes.Num()), Lease)) return nullptr;
		auto Exclusions = MakeShared<FVoxelTerrainCoarseExclusions, ESPMode::ThreadSafe>();
		Exclusions->Lease = MoveTemp(Lease);
		Exclusions->Boxes.Reserve(Boxes.Num());
		Exclusions->Boxes.Append(Boxes.GetData(), Boxes.Num());
		FVoxelTerrainCoarsePatchBuilder Builder(Resources, Consumer);
		FString Error;
		if (!Builder.Initialize(Geometry, Base, Exclusions, Changes, Error)) return nullptr;
		TAtomic<bool> Cancel{false};
		for (int32 Slice = 0; Slice < 20000; ++Slice)
		{
			const auto Progress = Builder.Advance(1, Cancel, Error);
			if (Progress == EVoxelTerrainCoarseMeshProgress::Complete) return Builder.TakeResult();
			if (Progress != EVoxelTerrainCoarseMeshProgress::Working) return nullptr;
		}
		return nullptr;
	}

	double PieceArea(const TSharedPtr<const FVoxelTerrainCoarsePatchPiece, ESPMode::ThreadSafe>& First)
	{
		double Area = 0;
		for (auto Piece = First; Piece; Piece = Piece->Next)
		{
			const auto& Mesh = Piece->Mesh->Batches[0].Mesh;
			for (int32 Index = 0; Index < Mesh.Triangles.Num(); Index += 3)
				Area += FVector::CrossProduct(Mesh.Vertices[Mesh.Triangles[Index + 1]] - Mesh.Vertices[Mesh.Triangles[Index]],
					Mesh.Vertices[Mesh.Triangles[Index + 2]] - Mesh.Vertices[Mesh.Triangles[Index]]).Size() * 0.5;
		}
		return Area;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainCoarseRangeSharingTest,
	"WHFramework.Voxel.FineRefactor.Rendering.CoarseRangeSharing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainCoarseRangeSharingTest::RunTest(const FString& Parameters)
{
	auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	auto Source = MakeSource();
	const auto Geometry = BuildGeometry(Resources, Consumer, Source);
	if (!TestTrue(TEXT("共享范围构建完成"), Geometry.IsValid())) return false;
	TestTrue(TEXT("保留原始几何身份及引用"), Geometry->Source == Source && Geometry->Stamp == Source->Stamp);
	TestEqual(TEXT("百万格跨度仅有两个32三角形叶子及根"), Geometry->Ranges.Num(), 3);
	TestTrue(TEXT("范围元数据按实际分配收费"), Geometry->MetadataLease->Bytes() >= Geometry->GetAllocatedBytes());
	const FBox NearBox(FVector(0, 0, -1), FVector(8, 8, 1));
	const FBox FarBox(FVector(1000000, 0, -1), FVector(1000008, 8, 1));
	const auto Near = BuildPatch(Resources, Consumer, Geometry, nullptr, MakeArrayView(&NearBox, 1), MakeArrayView(&NearBox, 1));
	if (!TestTrue(TEXT("局部裁切完成"), Near && Near->Roots[0])) return false;
	const auto NearLeaf = Near->Roots[0]->Children[0];
	TestTrue(TEXT("远处叶子直接共享原始索引范围"), !Near->Roots[0]->Children[1]);
	if (!TestTrue(TEXT("近处有裁切后的叶子"), NearLeaf && NearLeaf->FirstPiece)) return false;
	TestTrue(TEXT("只扣除盒内三角形面积"), FMath::IsNearlyEqual(PieceArea(NearLeaf->FirstPiece), 32.0 * 64.0, 1.0e-6));
	for (auto Piece = NearLeaf->FirstPiece; Piece; Piece = Piece->Next)
	{
		TestTrue(TEXT("局部 Piece 有效且不超过8MiB"), Piece->Mesh->bValidated && Piece->Mesh->ResourceLease->Bytes() <= 8ull * 1024 * 1024);
		const auto& Mesh = Piece->Mesh->Batches[0].Mesh;
		for (int32 Vertex = 0; Vertex < Mesh.Vertices.Num(); ++Vertex)
		{
			const FVector2D XY(Mesh.Vertices[Vertex].X, Mesh.Vertices[Vertex].Y);
			if (!TestTrue(TEXT("插值保留三套UV及法线"), Mesh.UV0[Vertex].Equals(XY) && Mesh.UV1[Vertex].Equals(XY * 2) &&
				Mesh.UV2[Vertex].Equals(XY * 3) && Mesh.Normals[Vertex].Equals(FVector::UpVector))) return false;
		}
	}
	const FBox BothBoxes[] = {NearBox, FarBox};
	const auto Both = BuildPatch(Resources, Consumer, Geometry, Near, BothBoxes, MakeArrayView(&FarBox, 1));
	if (!TestTrue(TEXT("远处变化完成"), Both && Both->Roots[0])) return false;
	TestTrue(TEXT("近处已裁切叶子保持同一不可变引用"), Both->Roots[0]->Children[0] == NearLeaf);
	const auto Restored = BuildPatch(Resources, Consumer, Geometry, Both, MakeArrayView(&FarBox, 1), MakeArrayView(&NearBox, 1));
	if (!TestTrue(TEXT("撤销近处覆盖完成"), Restored && Restored->Roots[0])) return false;
	TestTrue(TEXT("撤销后恢复原始索引范围而非重复裁切"), !Restored->Roots[0]->Children[0]);
	TestTrue(TEXT("远处未变叶子保留引用"), Restored->Roots[0]->Children[1] == Both->Roots[0]->Children[1]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainCoarseCapacityRecoveryTest,
	"WHFramework.Voxel.FineRefactor.Rendering.CoarseCapacityRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainCoarseCapacityRecoveryTest::RunTest(const FString& Parameters)
{
	auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	auto Source = MakeSource();
	FVoxelTerrainCoarseMeshBuilder Builder(Resources, Consumer);
	FString Error;
	if (!TestTrue(TEXT("初始化不展开空间 Atom"), Builder.Initialize(Source, FVector::ZeroVector, 1000000.0, Source->Stamp, Error))) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	TestTrue(TEXT("占满Staged容量"), Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, Resources->Snapshot().Limits.StagedBytes, Blocker));
	TAtomic<bool> Cancel{false};
	TestEqual(TEXT("分配前等待容量"), Builder.Advance(1, Cancel, Error), EVoxelTerrainCoarseMeshProgress::CapacityBlocked);
	Blocker.Reset();
	EVoxelTerrainCoarseMeshProgress Progress = EVoxelTerrainCoarseMeshProgress::Working;
	for (int32 Slice = 0; Slice < 1024 && Progress == EVoxelTerrainCoarseMeshProgress::Working; ++Slice) Progress = Builder.Advance(1, Cancel, Error);
	TestEqual(TEXT("容量释放后同一构建继续完成"), Progress, EVoxelTerrainCoarseMeshProgress::Complete);
	const auto Geometry = Builder.TakeResult();
	TestTrue(TEXT("空间尺度不会扩大几何或节点数"), Geometry && Geometry->Ranges.Num() == 3 && Geometry->Source == Source);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainCoarseCompletionCapacityTest,
	"WHFramework.Voxel.FineRefactor.Rendering.CoarseCompletionCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainCoarseCompletionCapacityTest::RunTest(const FString& Parameters)
{
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 1024 * 1024;
	Limits.WorkingBytes = 512 * 1024;
	Limits.StagedBytes = 2 * 1024 * 1024;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	const auto External = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!TestTrue(TEXT("创建生产容量账本"), FVoxelTerrainResources::Create(Limits, Consumer, Resources, Error))) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata;
	if (!TestTrue(TEXT("控制对象先授额"), Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelCoarseBuildStorage>(), Metadata))) return false;
	auto Storage = VoxelBoundedAllocation::MakeWorkerShared<FVoxelCoarseBuildStorage>(Metadata,
		Resources.ToSharedRef(), Consumer, Metadata, nullptr);
	const uint64 ArrayCapacity = VoxelBoundedAllocation::ArrayBytes<FVoxelDistantCell>(4096);
	Storage->GeometryCapacityBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSurfaceTileData>() + ArrayCapacity;
	Storage->WorkingCapacityBytes = 4096;
	FVoxelTaskResult Result;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
	if (!TestTrue(TEXT("原切点授予正文和微步容量"), Storage->Begin(Result, Working))) return false;
	auto Data = Storage->MakeResult<FVoxelSurfaceTileData>();
	Data->DistantCells.Reserve(4096);
	Data->DistantCells.SetNum(1024);
	Data->DistantCells[1023].Min = FIntVector(31, -17, 99);
	const uint64 OriginalSerial = Data->ResourceLease->Serial();
	const uint64 OriginalBytes = Data->ResourceLease->Bytes();
	const int32 OriginalArrayCapacity = Data->DistantCells.Max();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
	const auto BeforePressure = Resources->Snapshot();
	if (!TestTrue(TEXT("外部任务占满剩余 Working"), Resources->Acquire(External, EVoxelTerrainResourcePool::Working,
		Limits.WorkingBytes - BeforePressure.Used[int32(EVoxelTerrainResourcePool::Working)], Pressure))) return false;
	TestFalse(TEXT("压缩峰值无额度时保留原数组"), Storage->CompactGeometryArray(Data->DistantCells, ArrayCapacity, Working, Result));
	TestTrue(TEXT("同一任务等待几何容量而非终止或重建"), Result.bSuccess && Result.bContinue && Result.Error.IsEmpty());
	TestEqual(TEXT("峰值阻塞期间没有提前退款"), Data->ResourceLease->Bytes(), OriginalBytes);
	TestEqual(TEXT("峰值阻塞期间没有释放正文数组"), Data->DistantCells.Max(), OriginalArrayCapacity);
	Working.Reset();
	TestEqual(TEXT("自己的微步释放不能误唤醒原容量等待"), Storage->BlockReason(), EVoxelTaskBlockReason::Memory);
	Pressure.Reset();
	TestEqual(TEXT("外部 Working 释放唤醒同一切点"), Storage->BlockReason(), EVoxelTaskBlockReason::None);
	Result = {};
	if (!TestTrue(TEXT("同一构建重新取得微步容量"), Storage->Begin(Result, Working))) return false;
	if (!TestTrue(TEXT("峰值授额后压缩真实数组并退款"),
		Storage->CompactGeometryArray(Data->DistantCells, ArrayCapacity, Working, Result))) return false;
	TestEqual(TEXT("压缩保持原正文租约身份"), Data->ResourceLease->Serial(), OriginalSerial);
	TestEqual(TEXT("压缩不提前提升 Resident"), Data->ResourceLease->GetPool(), EVoxelTerrainResourcePool::Staged);
	TestTrue(TEXT("旧分配释放后 Staged 额度下降"), Data->ResourceLease->Bytes() < OriginalBytes);
	TestTrue(TEXT("原数组内容和数量保持"), Data->DistantCells.Num() == 1024 && Data->DistantCells[1023].Min == FIntVector(31, -17, 99));
	TestTrue(TEXT("最终正文仍完整收费"), Data->ResourceLease->Bytes() >=
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSurfaceTileData>() + Data->DistantCells.GetAllocatedSize());
	TestEqual(TEXT("容量回收没有增大原 Input 上限"), Resources->Snapshot().Limits.InputBytes, Limits.InputBytes);
	const uint64 RetainedArrayBytes = VoxelBoundedAllocation::QuantizeBytes(Data->DistantCells.GetAllocatedSize());
	Data->DistantCells.SetNum(0, EAllowShrinking::No);
	Working.Reset();
	Result = {};
	if (!TestTrue(TEXT("空页面不申请重分配峰值即可释放真实数组"),
		Storage->CompactGeometryArray(Data->DistantCells, RetainedArrayBytes, Working, Result))) return false;
	TestEqual(TEXT("空远景数组不保留最大分配"), Data->DistantCells.Max(), 0);
	TestEqual(TEXT("空页面仅保留原数据控制对象额度"), Data->ResourceLease->Bytes(),
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSurfaceTileData>());

	FVoxelTerrainGeometry Geometry;
	Geometry.Key = {EVoxelTerrainRepresentation::Surface, FIntVector::ZeroValue, 5};
	Geometry.SampleStepCells = 4;
	int32 ShardCount = 0;
	TArray<FVoxelTerrainBuildShard> Shards;
	if (!TestTrue(TEXT("同一空间几何产生一致的计数和分片"),
		VoxelStreamingGeometry::CountShards(Geometry, 32, 4096, ShardCount, Error) &&
		VoxelStreamingGeometry::BuildShards(Geometry, 32, 4096, Shards, Error))) return false;
	TestEqual(TEXT("元数据预算复用原分片数量"), ShardCount, Shards.Num());
	TestEqual(TEXT("128 采样列只生成16个初始窗口"), ShardCount, 16);
	TestFalse(TEXT("原分片硬上限仍然生效"), VoxelStreamingGeometry::CountShards(Geometry, 32, 15, ShardCount, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainCoarseResultInputLifetimeTest,
	"WHFramework.Voxel.FineRefactor.Rendering.CoarseResultInputLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainCoarseResultInputLifetimeTest::RunTest(const FString& Parameters)
{
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 512 * 1024;
	Limits.WorkingBytes = 64 * 1024;
	Limits.StagedBytes = 1024 * 1024;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	const auto External = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!TestTrue(TEXT("创建生产输入账本"), FVoxelTerrainResources::Create(Limits, Consumer, Resources, Error))) return false;
	const uint64 ControllerBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelCoarseBuildStorage>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainBuildShard>(4096);
	const uint64 ResultBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTaskResult>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelFineOverlayReadResult>();
	TSharedPtr<FVoxelTaskResult, ESPMode::ThreadSafe> Cached;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
	EVoxelTerrainResourceAdmission BeforeRelease = EVoxelTerrainResourceAdmission::None;
	uint64 InputBeforeRelease = 0, OriginalGeometrySerial = 0, OriginalResultSerial = 0;
	bool bConstructed = false;
	// 最后一个构建引用在真实 Worker 上析构；缓存引用继续存活，不能依赖 GT 异步删除时序。
	const auto Worker = Async(EAsyncExecution::ThreadPool, [&]
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata, ResultInput;
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, ControllerBytes, Metadata) ||
			!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, ResultBytes, ResultInput)) return;
		auto Storage = VoxelBoundedAllocation::MakeWorkerShared<FVoxelCoarseBuildStorage>(Metadata,
			Resources.ToSharedRef(), Consumer, Metadata, ResultInput);
		TArray<FVoxelTerrainBuildShard> Shards;
		Shards.Reserve(4096);
		Storage->WorkingCapacityBytes = 4096;
		Storage->GeometryCapacityBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSurfaceTileData>() +
			VoxelBoundedAllocation::ArrayBytes<FVoxelDistantCell>(4);
		FVoxelTaskResult Result;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
		if (!Storage->Begin(Result, Working)) return;
		Result.Surface = Storage->MakeResult<FVoxelSurfaceTileData>();
		Result.Surface->DistantCells.Reserve(4);
		Result.Surface->DistantCells.SetNum(4);
		Result.Surface->DistantCells[3].Min = FIntVector(7, -11, 43);
		Result.InputResourceLease = Storage->ResultInputLease;
		Result.OverlayReadResult = VoxelBoundedAllocation::MakeWorkerShared<FVoxelFineOverlayReadResult>(Storage->ResultInputLease);
		OriginalGeometrySerial = Result.Surface->ResourceLease->Serial();
		OriginalResultSerial = Result.InputResourceLease->Serial();
		Cached = VoxelBoundedAllocation::MakeWorkerShared<FVoxelTaskResult>(Storage->ResultInputLease, MoveTemp(Result));
		const uint64 UsedInput = Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)];
		if (UsedInput + 16 * 1024 >= Limits.InputBytes || !Resources->Acquire(External,
			EVoxelTerrainResourcePool::Input, Limits.InputBytes - UsedInput - 16 * 1024, Pressure)) return;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Replacement;
		BeforeRelease = Resources->TryAcquire(External, EVoxelTerrainResourcePool::Input, ControllerBytes, Replacement);
		InputBeforeRelease = Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)];
		bConstructed = true;
	});
	Worker.Wait();
	if (!TestTrue(TEXT("Worker 已构建并缓存原完成结果"), bConstructed && Cached && Cached->Surface)) return false;
	TestEqual(TEXT("构建状态活跃时新控制数组等待输入容量"), BeforeRelease, EVoxelTerrainResourceAdmission::CapacityBlocked);
	const uint64 InputAfterRelease = Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Input)];
	TestTrue(TEXT("缓存存活时完整构建数组额度已释放"), InputBeforeRelease >= InputAfterRelease + ControllerBytes);
	TestEqual(TEXT("缓存只保留独立结果输入租约"), Cached->InputResourceLease->Serial(), OriginalResultSerial);
	TestEqual(TEXT("结果对象及空网格预留保持完整收费"), Cached->InputResourceLease->Bytes(), ResultBytes);
	TestEqual(TEXT("CPU 正文保持原租约"), Cached->Surface->ResourceLease->Serial(), OriginalGeometrySerial);
	TestEqual(TEXT("正文未因控制状态退出提前提升Resident"), Cached->Surface->ResourceLease->GetPool(), EVoxelTerrainResourcePool::Staged);
	TestTrue(TEXT("缓存的正文和Overlay结果仍存活"), Cached->OverlayReadResult &&
		Cached->Surface->DistantCells.Num() == 4 && Cached->Surface->DistantCells[3].Min == FIntVector(7, -11, 43));
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Replacement;
	TestTrue(TEXT("原输入上限内可接纳下一构建，不需丢弃完成正文"),
		Resources->Acquire(External, EVoxelTerrainResourcePool::Input, ControllerBytes, Replacement));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainCoarseCompletionAdmissionTest,
	"WHFramework.Voxel.FineRefactor.Rendering.CoarseCompletionAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainCoarseCompletionAdmissionTest::RunTest(const FString& Parameters)
{
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 64 * 1024;
	Limits.WorkingBytes = 64 * 1024;
	Limits.StagedBytes = 64 * 1024;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	const auto External = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!TestTrue(TEXT("创建原生接纳账本"), FVoxelTerrainResources::Create(Limits, Consumer, Resources, Error))) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata, ResultInput, Pressure;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelCoarseBuildStorage>(), Metadata) ||
		!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelTaskResult>(), ResultInput) ||
		!Resources->Acquire(External, EVoxelTerrainResourcePool::Staged, Limits.StagedBytes, Pressure)) return false;
	FVoxelCoarseBuildStorage Storage(Resources.ToSharedRef(), Consumer, Metadata, ResultInput);
	Storage.GeometryCapacityBytes = 8192;
	Storage.WorkingCapacityBytes = 4096;
	TestEqual(TEXT("完成容量不可用时在排队前拒绝接纳"), Storage.ReserveCompletion(), EVoxelTerrainResourceAdmission::CapacityBlocked);
	TestFalse(TEXT("被拒绝的新尝试未取得正文额度"), Storage.GeometryLease.IsValid());
	TestEqual(TEXT("完成容量阻塞不执行或占用Scratch"),
		Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	Pressure.Reset();
	if (!TestEqual(TEXT("容量释放后同一尝试可保留完成路径"),
		Storage.ReserveCompletion(), EVoxelTerrainResourceAdmission::Granted)) return false;
	const uint64 CompletionSerial = Storage.GeometryLease->Serial();
	TestEqual(TEXT("尚未运行时完整正文容量已在Staged授予"), Storage.GeometryLease->Bytes(), uint64(8192));
	TestEqual(TEXT("接纳完成路径不提前消费Worker额度"),
		Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	FVoxelTaskResult Result;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
	if (!TestTrue(TEXT("Worker 使用已接纳的完成额度"), Storage.Begin(Result, Working))) return false;
	TestEqual(TEXT("Worker 沿用原完成租约而非重新申请"), Storage.GeometryLease->Serial(), CompletionSerial);
	TestEqual(TEXT("完成容量只记一次"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Staged)], uint64(8192));
	TestEqual(TEXT("运行微步Scratch完整收费"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(4096));
	Working.Reset();
	TestEqual(TEXT("重复接纳保留原身份"), Storage.ReserveCompletion(), EVoxelTerrainResourceAdmission::Granted);
	TestEqual(TEXT("重复接纳不累积正文额度"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Staged)], uint64(8192));
	return true;
}
#endif
