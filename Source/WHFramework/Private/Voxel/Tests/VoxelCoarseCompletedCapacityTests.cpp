#include "Voxel/Rendering/VoxelCoarseBuildStorage.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Async/Async.h"
#include "Misc/AutomationTest.h"

namespace
{
	struct FCoarseCompletedTestPayload : FVoxelTaskCustomPayload
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
		TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> Mesh;
	};

	template<typename ElementType>
	uint64 RetainCoarseTestArray(TArray<ElementType>& InArray)
	{
		InArray.Reserve(128);
		InArray.SetNum(1);
		return InArray.GetAllocatedSize();
	}

	uint64 FillCoarseTestMesh(FVoxelSectionMeshResult& InMesh)
	{
		InMesh.Batches.Reserve(8);
		auto& Mesh = InMesh.Batches.AddDefaulted_GetRef().Mesh;
		Mesh.Vertices.Reserve(64);
		Mesh.Normals.Reserve(64);
		Mesh.Triangles.Reserve(64);
		Mesh.UV0.Reserve(64);
		Mesh.UV1.Reserve(64);
		Mesh.UV2.Reserve(64);
		Mesh.Colors.Reserve(64);
		Mesh.Tangents.Reserve(64);
		Mesh.Vertices.Append({FVector::ZeroVector, FVector::ForwardVector, FVector::RightVector});
		Mesh.Triangles.Append({0, 1, 2});
		for (int32 Vertex = 0; Vertex < 3; ++Vertex)
		{
			Mesh.Normals.Add(FVector::UpVector);
			Mesh.UV0.Add(FVector2D::ZeroVector);
			Mesh.UV1.Add(FVector2D::ZeroVector);
			Mesh.UV2.Add(FVector2D::ZeroVector);
			Mesh.Colors.Add(FLinearColor::White);
			Mesh.Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
		}
		InMesh.ValidateAndCache();
		return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>() + InMesh.AllocatedBytes();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoarseCompletedCapacityTest,
	"WHFramework.Voxel.FineRefactor.Rendering.CoarseCompletedCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoarseCompletedCapacityTest::RunTest(const FString& Parameters)
{
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 1024 * 1024;
	Limits.WorkingBytes = 64 * 1024;
	Limits.StagedBytes = 1024 * 1024;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	const auto External = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!TestTrue(TEXT("创建生产完成容量账本"), FVoxelTerrainResources::Create(Limits, Consumer, Resources, Error)))
	{
		return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelCoarseBuildStorage>(), Metadata))
	{
		return false;
	}
	FVoxelCoarseBuildStorage Storage(Resources.ToSharedRef(), Consumer, Metadata, nullptr);
	Storage.GeometryCapacityBytes = 256 * 1024;
	Storage.WorkingCapacityBytes = 4096;
	FVoxelTaskResult Result;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
	if (!Storage.Begin(Result, Working))
	{
		return false;
	}
	const uint64 OriginalSerial = Storage.GeometryLease->Serial();
	const uint64 Reserved = Storage.GeometryLease->Bytes();
	Result.VoxelProxy = Storage.MakeResult<FVoxelVoxelProxyData>();
	Result.Surface = Storage.MakeResult<FVoxelSurfaceTileData>();
	Result.Water = Storage.MakeResult<FVoxelWaterSurfaceTileData>();
	Result.Macro = Storage.MakeResult<FVoxelMacroTileData>();
	uint64 MinimumRetained = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelVoxelProxyData>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSurfaceTileData>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelWaterSurfaceTileData>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelMacroTileData>();
	Result.VoxelProxy->Cells.InitializeColumns(16, FVoxelBlockState());
	MinimumRetained += Result.VoxelProxy->Cells.GetAllocatedSize();
	MinimumRetained += RetainCoarseTestArray(Result.VoxelProxy->TopSurfaceCoverage);
	for (auto& Halo : Result.VoxelProxy->Halo)
	{
		MinimumRetained += RetainCoarseTestArray(Halo);
	}
	MinimumRetained += RetainCoarseTestArray(Result.Surface->GroundZ);
	MinimumRetained += RetainCoarseTestArray(Result.Surface->WaterZ);
	MinimumRetained += RetainCoarseTestArray(Result.Surface->SurfaceStates);
	MinimumRetained += RetainCoarseTestArray(Result.Surface->Biome);
	MinimumRetained += RetainCoarseTestArray(Result.Surface->Flags);
	MinimumRetained += RetainCoarseTestArray(Result.Surface->DistantCells);
	Result.Surface->DistantCells[0].Min = FIntVector(7, -13, 91);
	MinimumRetained += RetainCoarseTestArray(Result.Water->GroundZ);
	MinimumRetained += RetainCoarseTestArray(Result.Water->WaterZ);
	MinimumRetained += RetainCoarseTestArray(Result.Water->WaterKind);
	MinimumRetained += RetainCoarseTestArray(Result.Macro->Height);
	MinimumRetained += RetainCoarseTestArray(Result.Macro->WaterHeight);
	MinimumRetained += RetainCoarseTestArray(Result.Macro->WaterKind);
	MinimumRetained += RetainCoarseTestArray(Result.Macro->SurfaceStates);
	MinimumRetained += RetainCoarseTestArray(Result.Macro->ForestCoverage);
	MinimumRetained += RetainCoarseTestArray(Result.Macro->LargeStructures);
	MinimumRetained += RetainCoarseTestArray(Result.Macro->DistantCells);
	Result.VoxelProxyMesh = Storage.MakeResult<FVoxelSectionMeshResult>();
	Result.VolumeBoundaryMesh = Storage.MakeResult<FVoxelSectionMeshResult>();
	MinimumRetained += FillCoarseTestMesh(*Result.VoxelProxyMesh);
	MinimumRetained += FillCoarseTestMesh(*Result.VolumeBoundaryMesh);
	Result.SurfaceMesh = Result.VoxelProxyMesh;
	Result.WaterMesh = Result.VolumeBoundaryMesh;
	Result.MacroMesh = Result.VoxelProxyMesh;
	auto Payload = Storage.MakeResult<FCoarseCompletedTestPayload>();
	Payload->Mesh = Storage.MakeResult<FVoxelSectionMeshResult>();
	MinimumRetained += VoxelBoundedAllocation::WorkerSharedBytes<FCoarseCompletedTestPayload>() + FillCoarseTestMesh(*Payload->Mesh);
	Result.CustomPayload = Payload;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ForeignLease;
	if (!Resources->Acquire(External, EVoxelTerrainResourcePool::Staged, 64 * 1024, ForeignLease))
	{
		return false;
	}
	auto ForeignData = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSurfaceTileData>(ForeignLease);
	ForeignData->ResourceLease = ForeignLease;
	ForeignData->GroundZ.Reserve(1024);
	ForeignData->GroundZ.SetNum(1024);
	ForeignData->GroundZ[1023] = 97;
	Result.VoxelProxyMesh->FrozenSurface = ForeignData;
	Result.FineMesh = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSectionMeshResult>(ForeignLease);
	Result.FineMesh->ResourceLease = ForeignLease;
	FillCoarseTestMesh(*Result.FineMesh);
	const uint64 ForeignSerial = ForeignLease->Serial();
	const uint64 ForeignBytes = ForeignLease->Bytes();
	Result.bSuccess = Result.bContinue = true;
	TestFalse(TEXT("未完成微步不退款"), Storage.SettleCompleted(Result));
	TestEqual(TEXT("续跑保留全部完成预留"), Storage.GeometryLease->Bytes(), Reserved);
	Result.bContinue = false;
	Result.bCanceled = true;
	TestFalse(TEXT("取消仍由原资源销毁路径结算"), Storage.SettleCompleted(Result));
	TestEqual(TEXT("取消不能提前释放存活数组额度"), Storage.GeometryLease->Bytes(), Reserved);
	Result.bCanceled = false;
	TestFalse(TEXT("溢出完成计量拒绝结算"), Storage.SettleCompleted(Result, Payload->Mesh.Get(), MAX_uint64));
	TestEqual(TEXT("错误计量保留原完成预留"), Storage.GeometryLease->Bytes(), Reserved);
	Result.Error.Reset();
	Result.bSuccess = true;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Pressure;
	const uint64 BeforePressure = Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Staged)];
	if (!Resources->Acquire(External, EVoxelTerrainResourcePool::Staged, Limits.StagedBytes - BeforePressure - 32 * 1024, Pressure))
	{
		return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Next;
	TestEqual(TEXT("粗层持有未结算预留时下一发布等待容量"),
		Resources->TryAcquire(External, EVoxelTerrainResourcePool::Staged, 128 * 1024, Next), EVoxelTerrainResourceAdmission::CapacityBlocked);
	if (!TestTrue(TEXT("完成后结算所有原正文对象与实际分配"), Storage.SettleCompleted(Result,
		Payload->Mesh.Get(), VoxelBoundedAllocation::WorkerSharedBytes<FCoarseCompletedTestPayload>())))
	{
		return false;
	}
	const uint64 Settled = Storage.GeometryLease->Bytes();
	TestTrue(TEXT("真实数组容量及销毁任务额度完整保留"), Settled >= MinimumRetained);
	TestTrue(TEXT("完成后退回未使用的预留"), Settled < Reserved);
	TestTrue(TEXT("网格计量包含容量Slack而非仅逻辑顶点"), Settled > Result.VoxelProxyMesh->Bytes() + Result.VolumeBoundaryMesh->Bytes());
	TestEqual(TEXT("结算保持原正文资源身份"), Storage.GeometryLease->Serial(), OriginalSerial);
	TestEqual(TEXT("完成后仍在Staged等待真实发布"), Storage.GeometryLease->GetPool(), EVoxelTerrainResourcePool::Staged);
	TestEqual(TEXT("冻结输入保持独立租约身份"), ForeignLease->Serial(), ForeignSerial);
	TestEqual(TEXT("冻结输入与外部网格不退款或重复收费"), ForeignLease->Bytes(), ForeignBytes);
	TestTrue(TEXT("冻结输入和输出内容保持"), ForeignData->GroundZ[1023] == 97 &&
		Result.Surface->DistantCells[0].Min == FIntVector(7, -13, 91) && Result.VoxelProxyMesh->bValidated && Result.VolumeBoundaryMesh->bValidated);
	TestTrue(TEXT("原容量内接纳下一发布而保留当前全部正文"),
		Resources->Acquire(External, EVoxelTerrainResourcePool::Staged, 128 * 1024, Next));
	Result.SurfaceMesh.Reset();
	Result.WaterMesh.Reset();
	Result.MacroMesh.Reset();
	TestTrue(TEXT("别名引用消失仍可幂等结算"), Storage.SettleCompleted(Result,
		Payload->Mesh.Get(), VoxelBoundedAllocation::WorkerSharedBytes<FCoarseCompletedTestPayload>()));
	TestEqual(TEXT("同一正文对象只收费一次"), Storage.GeometryLease->Bytes(), Settled);
	TestEqual(TEXT("原Stage上限保持"), Resources->Snapshot().Limits.StagedBytes, Limits.StagedBytes);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoarseRetainedCompletionCapacityTest,
	"WHFramework.Voxel.FineRefactor.Rendering.CoarseRetainedCompletionCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoarseRetainedCompletionCapacityTest::RunTest(const FString& Parameters)
{
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 2 * 1024 * 1024;
	Limits.WorkingBytes = 64 * 1024;
	Limits.StagedBytes = 2 * 1024 * 1024;
	const auto Control = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!TestTrue(TEXT("创建固定容量生产账本"), FVoxelTerrainResources::Create(Limits, Control, Resources, Error)))
	{
		return false;
	}
	TArray<FVoxelTaskResult> Results;
	Results.Reserve(256);
	bool bCompleted = true;
	uint64 PeakStaged = 0;
	const auto Worker = Async(EAsyncExecution::ThreadPool, [&]
	{
		for (int32 Index = 0; Index < 256; ++Index)
		{
			const auto Consumer = FVoxelGenerationConsumerKey::Section(1, Control.OwnerId, 1, FIntVector(Index, 0, 0), uint64(Index + 1));
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata;
			if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
				VoxelBoundedAllocation::WorkerSharedBytes<FVoxelCoarseBuildStorage>(), Metadata))
			{
				bCompleted = false;
				return;
			}
			auto Storage = VoxelBoundedAllocation::MakeWorkerShared<FVoxelCoarseBuildStorage>(Metadata,
				Resources.ToSharedRef(), Consumer, Metadata, nullptr);
			Storage->GeometryCapacityBytes = 256 * 1024;
			Storage->WorkingCapacityBytes = 4096;
			FVoxelTaskResult Result;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
			if (!Storage->Begin(Result, Working))
			{
				bCompleted = false;
				return;
			}
			PeakStaged = FMath::Max(PeakStaged, Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Staged)]);
			Result.Surface = Storage->MakeResult<FVoxelSurfaceTileData>();
			Result.Surface->DistantCells.Reserve(4);
			Result.Surface->DistantCells.SetNum(4);
			Result.Surface->DistantCells[3].Min = FIntVector(Index, -7, 31);
			Result.bSuccess = true;
			if (!Storage->SettleCompleted(Result))
			{
				bCompleted = false;
				return;
			}
			Results.Add(MoveTemp(Result));
		}
	});
	Worker.Wait();
	if (!TestTrue(TEXT("原2MiB上限内连续接纳并保留256个完成页面"), bCompleted && Results.Num() == 256))
	{
		return false;
	}
	TestTrue(TEXT("构建与所有存活输出的合并峰值不超原上限"), PeakStaged <= Limits.StagedBytes);
	for (int32 Index = 0; Index < Results.Num(); ++Index)
	{
		const auto& Surface = Results[Index].Surface;
		if (!TestTrue(TEXT("未提交页面仍保留原正文和Staged租约"), Surface &&
			Surface->DistantCells[3].Min == FIntVector(Index, -7, 31) &&
			Surface->ResourceLease->GetPool() == EVoxelTerrainResourcePool::Staged &&
			Surface->ResourceLease->Bytes() >= VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSurfaceTileData>() + Surface->DistantCells.GetAllocatedSize()))
		{
			return false;
		}
	}
	TestEqual(TEXT("没有借用Resident容量完成生产"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Resident)], uint64(0));
	const auto ReleaseWorker = Async(EAsyncExecution::ThreadPool, [&Results]
	{
		Results.Empty();
	});
	ReleaseWorker.Wait();
	TestEqual(TEXT("原销毁路径释放所有完成正文"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Staged)], uint64(0));
	TestEqual(TEXT("固定容量上限保持"), Resources->Snapshot().Limits.StagedBytes, Limits.StagedBytes);
	return true;
}
#endif
