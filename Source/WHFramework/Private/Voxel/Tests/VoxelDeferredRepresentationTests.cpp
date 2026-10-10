#include "Voxel/Rendering/VoxelViewManager.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/VoxelModule.h"

namespace
{
	FVoxelGameThreadContinuation HoldCoverage()
	{
		co_await FVoxelGameThreadContinuation::Yield();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelDeferredRepresentationAdmissionTest,
	"WHFramework.Voxel.FineRefactor.Rendering.DeferredRepresentationAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelDeferredRepresentationAdmissionTest::RunTest(const FString& Parameters)
{
	const uint64 Epoch = 37;
	const auto Control = FVoxelGenerationConsumerKey::Section(Epoch, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources, Facts;
	TUniquePtr<FVoxelTaskScheduler> SchedulerOwner;
	FString Error;
	if (!TestTrue(TEXT("生产账本及Scheduler先授额"),
		FVoxelTerrainResources::Create({}, Control, Resources, Error) &&
		FVoxelTerrainResources::Create({}, Control, Facts, Error) &&
		FVoxelTaskScheduler::Create(Resources.ToSharedRef(), Facts.ToSharedRef(), Control, SchedulerOwner, Error) &&
		SchedulerOwner->InitializeStatistics(Epoch))) return false;
	auto& Scheduler = *SchedulerOwner;
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 2;
	Budget.MaxCompletedResultsPerFrame = 4;
	Scheduler.SetBudget(Budget);
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelViewManager Manager(*Module, Scheduler, Epoch);
	TAtomic<int32> Executed{0};
	int32 Applied = 0;
	int32 StaleApplied = 0;
	uint64 ConsumedSerial = 0;
	ON_SCOPE_EXIT { Scheduler.StopAndJoin(); };

	for (const auto Representation : {EVoxelTerrainRepresentation::VoxelProxy, EVoxelTerrainRepresentation::Surface, EVoxelTerrainRepresentation::Macro})
	{
		FVoxelTerrainGeometry Geometry;
		Geometry.Key = {Representation, FIntVector::ZeroValue, 3};
		Geometry.SampleStepCells = 4;
		Geometry.WorldMinZ = -32;
		Geometry.WorldMaxZ = 64;
		if (!TestTrue(TEXT("完整空间几何有效"), Geometry.Validate(Error))) return false;
		const auto Stamp = Manager.MakeRepresentationStamp(Geometry, 7);
		const auto Kind = Representation == EVoxelTerrainRepresentation::VoxelProxy ? EVoxelTaskKind::BuildVoxelProxy :
			Representation == EVoxelTerrainRepresentation::Surface ? EVoxelTaskKind::BuildSurface : EVoxelTaskKind::BuildMacro;
		const auto DataKind = Representation == EVoxelTerrainRepresentation::VoxelProxy ? EVoxelTaskKind::GenerateVoxelProxy :
			Representation == EVoxelTerrainRepresentation::Surface ? EVoxelTaskKind::GenerateSurface : EVoxelTaskKind::GenerateMacro;
		const auto MakeRequest = [&](const FVoxelTaskStamp& InStamp, const EVoxelTaskKind InKind, const bool bSuccess, const bool bDefaultApply = false)
		{
			FVoxelTaskRequest Request;
			Request.Kind = InKind;
			Request.Stamp = InStamp;
			Request.GenerationConsumer = FVoxelGenerationConsumerKey::Section(Epoch, InStamp.OwnerId, InStamp.OwnerIncarnation, {}, InStamp.Token);
			Request.ReservedBytes = 1;
			Request.WorkClass = EVoxelWorkClass::Critical;
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Output;
			if (!Resources->Acquire(Request.GenerationConsumer, EVoxelTerrainResourcePool::Staged,
				VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>() +
				VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(1), Output)) return Request;
			if (!Request.BindExecute(Resources.ToSharedRef(), Request.GenerationConsumer,
				[Output, Kind, bSuccess, &Executed](const TAtomic<bool>&)
				{
					++Executed;
					FVoxelTaskResult Result;
					Result.bSuccess = bSuccess;
					auto Mesh = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSectionMeshResult>(Output);
					Mesh->ResourceLease = Output;
					Mesh->Batches.Reserve(1);
					Mesh->ValidateAndCache();
					if (Kind == EVoxelTaskKind::BuildVoxelProxy) Result.VoxelProxyMesh = Mesh;
					else if (Kind == EVoxelTaskKind::BuildSurface) Result.SurfaceMesh = Mesh;
					else Result.MacroMesh = Mesh;
					return Result;
				}, Error)) return Request;
			if (bDefaultApply) return Request;
			Request.BindApply(Resources.ToSharedRef(), Request.GenerationConsumer,
				[this, &Manager, &Applied, &StaleApplied, &ConsumedSerial, Kind](FVoxelTaskResult&& Result)
				{
					TestFalse(TEXT("消费原结果前解除同一待回调状态"), Manager.HasDeferredRepresentation(Result.Stamp));
					if (Result.bCanceled)
					{
						++Applied;
						return;
					}
					if (!Manager.IsRepresentationStampCurrent(Result.Stamp)) ++StaleApplied;
					const auto Mesh = Kind == EVoxelTaskKind::BuildVoxelProxy ? Result.VoxelProxyMesh :
						Kind == EVoxelTaskKind::BuildSurface ? Result.SurfaceMesh : Result.MacroMesh;
					TestTrue(TEXT("延期保留真实完成正文及原Staged租约"), Mesh && Mesh->bValidated &&
						Mesh->ResourceLease->GetPool() == EVoxelTerrainResourcePool::Staged);
					if (Mesh) ConsumedSerial = Mesh->ResourceLease->Serial();
					++Applied;
				}, Error);
			return Request;
		};
		const auto WaitForHandoff = [&](const FVoxelTaskStamp& InStamp)
		{
			const double Deadline = FPlatformTime::Seconds() + 5.0;
			while (Scheduler.Has(InStamp, Kind) && FPlatformTime::Seconds() < Deadline)
			{
				Scheduler.Tick([](FVoxelTaskResult&&) {}, 10.0);
				FPlatformProcess::Sleep(0.001f);
			}
			Scheduler.Tick([](FVoxelTaskResult&&) {}, 10.0);
			return !Scheduler.Has(InStamp, Kind);
		};
		Manager.CoverageContinuation = HoldCoverage();
		Manager.bActiveCoverageFull = true;
		const int32 BeforeDefaultExecuted = Executed.Load();
		if (!TestTrue(TEXT("真实本地默认分发请求进入Scheduler"), Manager.EnqueueViewTask(MakeRequest(Stamp, Kind, true, true))) ||
			!TestTrue(TEXT("默认分发交出原Worker结果"), WaitForHandoff(Stamp))) return false;
		if (!TestTrue(TEXT("默认分发同样保留完成交接"), Manager.HasDeferredRepresentation(Stamp))) return false;
		TestTrue(TEXT("默认分发不落入另一个无身份延期数组"), Manager.DeferredCoverageResults.IsEmpty());
		TestFalse(TEXT("默认分发等待期间禁止再次接纳网格"), Manager.EnqueueViewTask(MakeRequest(Stamp, Kind, true, true)));
		TestFalse(TEXT("默认分发等待期间禁止再次生成数据"), Manager.EnqueuePreparedData(MakeRequest(Stamp, DataKind, true)));
		TestEqual(TEXT("本地默认路径只执行原Worker一次"), Executed.Load(), BeforeDefaultExecuted + 1);
		FVoxelViewManager::FDeferredCoverageCallback DefaultCallback;
		if (!Manager.DeferredCoverageCallbacks.Dequeue(DefaultCallback)) return false;
		Manager.CoverageContinuation.Reset();
		DefaultCallback.Apply();
		DefaultCallback = {};
		TestFalse(TEXT("本地默认消费解除自身交接"), Manager.HasDeferredRepresentation(Stamp));
		Manager.CoverageContinuation = HoldCoverage();
		const int32 BeforeApplied = Applied;
		const int32 BeforeExecuted = Executed.Load();
		if (!TestTrue(TEXT("真实网格任务进入Scheduler"), Manager.EnqueueViewTask(MakeRequest(Stamp, Kind, true))) ||
			!TestTrue(TEXT("Worker完成后交出原结果"), WaitForHandoff(Stamp))) return false;
		TestEqual(TEXT("覆盖尚未解除时不调用发布消费"), Applied, BeforeApplied);
		if (!TestTrue(TEXT("Scheduler空闲后原请求仍有待消费结果"), Manager.HasDeferredRepresentation(Stamp))) return false;
		TestFalse(TEXT("等待覆盖不能伪造已发布网格"), Manager.IsAdmissionMeshReady(
			{Representation == EVoxelTerrainRepresentation::VoxelProxy ? EVoxelViewAdmissionKind::VoxelProxy :
				Representation == EVoxelTerrainRepresentation::Surface ? EVoxelViewAdmissionKind::Surface : EVoxelViewAdmissionKind::Macro}));
		TestFalse(TEXT("同一请求不能重复接纳网格"), Manager.EnqueueViewTask(MakeRequest(Stamp, Kind, true)));
		TestFalse(TEXT("同一待网格请求不能再次生成数据"), Manager.EnqueuePreparedData(MakeRequest(Stamp, DataKind, true)));
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 10.0);
		TestEqual(TEXT("等待期间真实Worker仅执行一次"), Executed.Load(), BeforeExecuted + 1);
		FVoxelViewManager::FDeferredCoverageCallback Callback;
		if (!TestTrue(TEXT("覆盖队列持有唯一原回调"), Manager.DeferredCoverageCallbacks.Dequeue(Callback))) return false;
		Manager.CoverageContinuation.Reset();
		Callback.Apply();
		Callback = {};
		TestFalse(TEXT("消费后同一请求退出延期状态"), Manager.HasDeferredRepresentation(Stamp));
		TestEqual(TEXT("原完成结果只消费一次"), Applied, BeforeApplied + 1);
		TestTrue(TEXT("原Staged租约保持身份至消费"), ConsumedSerial != 0);

		Manager.CoverageContinuation = HoldCoverage();
		if (!TestTrue(TEXT("消费后同一身份允许必要重试"), Manager.EnqueueViewTask(MakeRequest(Stamp, Kind, true))) ||
			!WaitForHandoff(Stamp)) return false;
		Geometry.SampleStepCells = 8;
		const auto Replacement = Manager.MakeRepresentationStamp(Geometry, 7);
		TestFalse(TEXT("几何变化不会沿用旧延期状态"), Manager.HasDeferredRepresentation(Replacement));
		if (!TestTrue(TEXT("新身份可以正常接纳"), Manager.EnqueueViewTask(MakeRequest(Replacement, Kind, true))) ||
			!WaitForHandoff(Replacement)) return false;
		if (!TestTrue(TEXT("新身份也保留自身完成交接"), Manager.HasDeferredRepresentation(Replacement))) return false;
		Manager.CoverageContinuation.Reset();
		if (!Manager.DeferredCoverageCallbacks.Dequeue(Callback)) return false;
		Callback.Apply();
		Callback = {};
		TestTrue(TEXT("旧回调不能清除新身份的延期状态"), Manager.HasDeferredRepresentation(Replacement));
		if (!Manager.DeferredCoverageCallbacks.Dequeue(Callback)) return false;
		Callback.Apply();
		Callback = {};
		TestFalse(TEXT("新回调只解除自己所属的请求"), Manager.HasDeferredRepresentation(Replacement));

		Manager.CoverageContinuation = HoldCoverage();
		if (!Manager.EnqueueViewTask(MakeRequest(Replacement, Kind, false)) || !WaitForHandoff(Replacement)) return false;
		TestFalse(TEXT("失败结果不能冒充已完成待消费网格"), Manager.HasDeferredRepresentation(Replacement));
		Manager.CoverageContinuation.Reset();
		if (!Manager.DeferredCoverageCallbacks.Dequeue(Callback)) return false;
		Callback.Apply();
		Callback = {};

		Manager.CoverageContinuation = HoldCoverage();
		if (!Manager.EnqueueViewTask(MakeRequest(Replacement, Kind, true))) return false;
		Scheduler.CancelMatching([&](const EVoxelTaskKind InKind, const FVoxelTaskStamp& InStamp)
		{
			return InKind == Kind && InStamp == Replacement;
		});
		if (!WaitForHandoff(Replacement)) return false;
		TestFalse(TEXT("取消结果不阻塞原身份后续尝试"), Manager.HasDeferredRepresentation(Replacement));
		Manager.CoverageContinuation.Reset();
		while (Manager.DeferredCoverageCallbacks.Dequeue(Callback))
		{
			Callback.Apply();
			Callback = {};
		}
		Manager.CoverageContinuation = HoldCoverage();
		if (!Manager.EnqueueViewTask(MakeRequest(Replacement, Kind, true)) || !WaitForHandoff(Replacement)) return false;
		if (!TestTrue(TEXT("Reset前原完成结果仍等待覆盖消费"), Manager.HasDeferredRepresentation(Replacement))) return false;
		Manager.Reset();
		TestFalse(TEXT("Reset解除待消费结果对应身份"), Manager.HasDeferredRepresentation(Replacement));
		TestTrue(TEXT("Reset撤销原覆盖回调"), Manager.DeferredCoverageCallbacks.IsEmpty());
	}
	TestEqual(TEXT("三种粗层均隔离旧几何回调"), StaleApplied, 3);
	Scheduler.StopAndJoin();
	Manager.Reset();
	TestTrue(TEXT("Reset清除原请求生命周期"), Manager.RepresentationAttempts.IsEmpty());
	return true;
}
#endif
