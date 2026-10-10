#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Components/SceneComponent.h"
#include "AI/Navigation/NavigationElement.h"
#include "AI/Navigation/NavigationRelevantData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "EngineUtils.h"
#include "EngineGlobals.h"
#include "HAL/PlatformProcess.h"
#include "Tasks/Task.h"
#include "Main/MainModule.h"
#include "SaveGame/SaveGameAsyncExecutor.h"
#include "SaveGame/SaveGameStorage.h"
#include "Voxel/Save/VoxelSceneColumnCodec.h"
#include "Voxel/Collision/VoxelCollisionPresenter.h"
#include "Voxel/Collision/VoxelPhysicsRetirement.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Navigation/VoxelNavigationPresenter.h"
#include "Voxel/Navigation/VoxelNavigationBounds.h"
#include "Voxel/Navigation/VoxelNavigationSurfaceComponent.h"
#include "Voxel/Scene/VoxelSceneRegion.h"
#include "Voxel/Streaming/VoxelResidencyManager.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include "Voxel/Tests/VoxelNavigationTestTypes.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Voxels/Data/VoxelData.h"

struct FVoxelNavigationSimulationDemandEvents final : FVoxelResidencyDemandEvents
{
	FVoxelInterestDelta Delta;
	virtual const FVoxelInterestDelta& GetResidencyDelta() const override
	{
		return Delta;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNavigationResidencyTest,
	"WHFramework.Voxel.Navigation.SimulationCollisionAndDataEviction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNavigationResidencyTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UWorld::InitializationValues WorldInitialization;
	WorldInitialization.CreatePhysicsScene(true).ShouldSimulatePhysics(true).CreateNavigation(true).CreateAISystem(true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &WorldInitialization);
	if (!TestNotNull(TEXT("Navigation fixture world is created"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AMainModule* Owner = World->SpawnActor<AMainModule>();
	UVoxelModule* Module = NewObject<UVoxelModule>(Owner);
	Owner->GetModules().Add(Module);
	Owner->GetModuleMap().Add(Module->GetModuleName(), Module);
	ON_SCOPE_EXIT
	{
		FString CleanupError;
		if (Module->SaveAdapter.IsBusy() || Module->SceneSaveBuilder.IsSet()) Module->OnAfterSaveData(false);
		Module->StopWorld(true, CleanupError);
		World->DestroyWorld(false);
#if WITH_EDITOR
		Module->OnDestroy();
#endif
		Owner->GetModuleMap().Reset();
		Owner->GetModules().Reset();
		GEngine->DestroyWorldContext(World);
	};
	if (!TestEqual(TEXT("Navigation fixture registers its actual singleton"), UVoxelModule::GetPtr(World), Module)) return false;
	if (!Owner->GetRootComponent())
	{
		USceneComponent* Root = NewObject<USceneComponent>(Owner);
		Owner->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	Module->bEnableProjectSceneSimulation = true;
	Module->SceneRegionClass = UVoxelNavigationTestSceneRegion::StaticClass();
	Module->Epoch = 11;
	Module->Manifest.BlockSizeCentimeters = 25;
	Module->NavigationClearanceCentimeters = 175.;
	Module->WorldState = EVoxelWorldState::Running;
	// 本夹具验证生命周期，而真实单步耗时上限由生产 Trace/性能验收单独验证。
	Module->MaximumSceneCaptureStepMilliseconds = 1000.0;
	UVoxelData* Support = NewObject<UVoxelData>();
	Support->BlockName = TEXT("test:navigation_support");
	Support->DropCount = 0;
	FString Error;
	if (!TestTrue(TEXT("Fixture uses the real block registry"), Module->Registry.Build({Support}, false, Error)))
	{
		AddError(Error);
		return false;
	}
	const uint16 SupportType = Module->Registry.GetSnapshot()->Find(Support->BlockName)->TypeId;
	const auto Shapes = MakeShared<FVoxelShapeRegistry, ESPMode::ThreadSafe>();
	Shapes->BuildDefaults();
	Module->Shapes = Shapes;
	Module->Generator = VoxelTest::MakeGenerator();
	Module->Runtime = MakeUnique<FVoxelWorldRuntime>(Module->Epoch, true, Module->Registry.GetSnapshot().ToSharedRef(), Module->Generator.ToSharedRef());
	const auto Control = FVoxelGenerationConsumerKey::Section(Module->Epoch, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> TerrainResources, RuntimeResources;
	if (!TestTrue(TEXT("导航夹具建立生产原控制账本"),
		FVoxelTerrainResources::Create({}, Control, TerrainResources, Error) &&
		FVoxelTerrainResources::Create({}, Control, RuntimeResources, Error) &&
		FVoxelTaskScheduler::Create(TerrainResources.ToSharedRef(), RuntimeResources.ToSharedRef(), Control, Module->Scheduler, Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestTrue(TEXT("真实Scheduler初始化原控制身份和统计"), Module->Scheduler->InitializeStatistics(Module->Epoch))) return false;
	Module->Runtime->BindTerrainResources(Module->Scheduler->GetRuntimeResources());
	if (!Module->TerrainFrameContext.Configure({}, Error)) return false;
	Module->Scheduler->BindFrameContext(Module->TerrainFrameContext);
	Module->CollisionPresenter = MakeUnique<FVoxelCollisionPresenter>(*Module, *Module->Scheduler, Module->Epoch);
	Module->NavigationPresenter = MakeUnique<FVoxelNavigationPresenter>(*Module, *Module->Scheduler);
	Module->ResidencyManager = MakeUnique<FVoxelResidencyManager>(*Module->Runtime, *Module->Scheduler);
	if (!TestTrue(TEXT("真实ResidencyManager按原控制身份初始化有界回调"), Module->ResidencyManager->Initialize(
		[Module](const FIntVector& Section) { Module->OnSectionEvicting(Section); }, [](const FIntVector&) { return false; },
		[Module](const FIntVector& Section, const FVoxelSectionStamp& Stamp, const EVoxelRuntimeSectionEvent Event)
		{
			Module->OnSimulationSectionEvent(Section, Stamp, Event);
		}, Error))) return false;
	Module->ResidencyManager->SetEvictGraceFrames(0);
	const FIntVector Lower(0, 0, 0);
	const FIntVector Above(0, 0, 1);
	const FIntVector Distant(3, 0, 0);
	auto PublishSection = [&](const FIntVector& Key, const int32 SupportHeight)
	{
		FVoxelSection* Section = Module->Runtime->FindOrAllocate(Key, 0);
		TArray<FVoxelBlockState> Blocks;
		Blocks.Init(FVoxelBlockState(), 4096);
		if (SupportHeight >= 0) Blocks[4 + 16 * (4 + 16 * SupportHeight)] = FVoxelBlockState(SupportType, 0);
		return Section && Module->Runtime->PublishBase(Key, Section->Stamp, MoveTemp(Blocks), Error) &&
			Module->Runtime->PublishFinal(Key, 0, {}, {}, Error);
	};
	if (!TestTrue(TEXT("Known support and upper clearance publish through the runtime"),
		PublishSection(Lower, 15) && PublishSection(Above, -1) && PublishSection(Distant, 0) &&
			PublishSection(Distant + FIntVector(0, 0, 1), -1)))
	{
		AddError(Error);
		return false;
	}
	const auto Interest = MakeShared<FVoxelInterestSet, ESPMode::ThreadSafe>();
	const auto Inputs = MakeShared<FVoxelTerrainPublicationInputs, ESPMode::ThreadSafe>();
	Inputs->RuntimeSections = {Lower, Above, Distant};
	Interest->PublicationInputs = Inputs;
	Module->CurrentInterest = Interest;
	const FIntVector RemovedPending(6, 0, 0);
	TestTrue(TEXT("未激活模拟票据的输入可以真实发布"), PublishSection(RemovedPending, 0));
	Interest->Exact.Add(RemovedPending).bSimulation = true;
	Module->WakeSimulationSection(RemovedPending);
	TestTrue(TEXT("局部协调票据随实际Section登记"), Module->Runtime->FindSection(RemovedPending)->bSimulationQueued);
	Interest->Exact.Remove(RemovedPending);
	TestTrue(TEXT("外部Runtime移除在释放正文前解绑模拟票据"), Module->Runtime->RemoveSection(RemovedPending));
	TestNull(TEXT("外部移除没有留下模拟队列悬空头"), Module->SimulationReadyHead);
	Interest->Exact.Add(Above, FVoxelExactDemand());
	for (const FIntVector& Key : {Lower, Distant})
	{
		FVoxelExactDemand& Demand = Interest->Exact.Add(Key);
		Demand.bSimulation = true;
		TestFalse(TEXT("Simulation cannot activate before actual collision publication"), Module->ActivateSimulationSection(Key));
		Demand.bSimulation = false;
		Demand.bCollision = true;
		Demand.ServicePriority = {EVoxelTerrainServiceBand::P0, 0.0, 0.0, 0, FGuid::NewGuid()};
	}
	auto FindSurface = [&](const FIntVector& Key) -> UVoxelNavigationSurfaceComponent*
	{
		TInlineComponentArray<UVoxelNavigationSurfaceComponent*> Components(Owner);
		for (UVoxelNavigationSurfaceComponent* Component : Components)
		{
			if (Component->GetComponentLocation().Equals(FVector(Key * 16) * Module->BlockSize())) return Component;
		}
		return nullptr;
	};
	uint64 TestFrame = 1;
	auto Pump = [&](TFunctionRef<bool()> Finished, const bool bPollInFrame = false)
	{
		const double Deadline = FPlatformTime::Seconds() + 10.;
		bool bFinished = false;
		do
		{
			Module->TerrainFrameContext.Begin(Module->Epoch, ++TestFrame, FPlatformTime::Seconds(), {});
			Module->CollisionPresenter->Tick(Interest->Exact, 1);
			Module->ReconcileSimulationDemand();
			Module->AdvanceSceneRegions();
			Module->NavigationPresenter->Tick();
			Module->Scheduler->Tick([Module](FVoxelTaskResult&& Result)
			{
				Module->CollisionPresenter->OnTask(MoveTemp(Result));
			}, 8.);
			if (const auto Retirement = Module->GetPhysicsRetirement()) Retirement->Advance(Module->TerrainFrameContext);
			if (bPollInFrame) bFinished = Finished();
			Module->TerrainFrameContext.End(FPlatformTime::Seconds());
			// 该夹具必须推进真实 Scene/PT 输入；仅等待 Worker 或轮询不会形成物理提交回执。
			World->Tick(LEVELTICK_All, 1.0f / 60.0f);
			// 同步 Automation 阻塞引擎主循环；显式排空真实 Solver 的命令和粒子输入。
			World->GetPhysicsScene()->Flush();
			FPlatformProcess::Sleep(0.001f);
			if (!bPollInFrame) bFinished = Finished();
		}
		while (!bFinished && FPlatformTime::Seconds() < Deadline);
		return bFinished;
	};
	if (!TestTrue(TEXT("Real collision jobs publish their physics components"), Pump([&]()
	{
		return Module->IsCollisionReady(Lower) && Module->IsCollisionReady(Distant);
	})))
	{
		const auto* Body = Module->CollisionPresenter->FindBody(Lower);
		FVoxelSectionSnapshot DiagnosticSnapshot;
		AddInfo(FString::Printf(TEXT("Collision pipeline: body=%d prepared=%d preparing=%d sync=%d failed=%d capacityBlocked=%d requestBlocked=%d selectionAdmission=%d schedulerAdmission=%d snapshot=%d retirement=%d"),
			Body != nullptr, Body && Body->Prepared.IsValid(), Body && Body->PreparingComponent.IsValid(), Body && Body->Sync.IsValid(),
			Body && Body->bHasFailure, Body && Body->bCapacityBlocked,
			Body && Body->AdmissionRequest.IsSet() ? int32(Body->AdmissionRequest->Blocked) : 0,
			int32(Module->CollisionPresenter->Selection.GetLastResourceAdmission()), int32(Module->Scheduler->GetLastResourceAdmission()),
			Module->Runtime->CaptureSnapshot(Lower, DiagnosticSnapshot), Module->GetPhysicsRetirement().IsValid()));
		return false;
	}
	TestNull(TEXT("Collision/data residency alone creates no navigation surface"), FindSurface(Lower));
	TestFalse(TEXT("Collision readiness without Simulation cannot activate a scene region"), Module->ActivateSimulationSection(Lower));
	for (const FIntVector& Key : {Lower, Distant})
		Interest->Exact.FindChecked(Key).bSimulation = true;
	const auto Added = MakeShared<FVoxelNavigationSimulationDemandEvents, ESPMode::ThreadSafe>();
	Added->Delta.AddedSimulation = {Lower, Distant};
	Added->Delta.AddedExact = {Lower, Distant};
	const auto Removed = MakeShared<FVoxelNavigationSimulationDemandEvents, ESPMode::ThreadSafe>();
	Removed->Delta.RemovedSimulation = {Lower};
	const auto Revived = MakeShared<FVoxelNavigationSimulationDemandEvents, ESPMode::ThreadSafe>();
	Revived->Delta.AddedSimulation = {Lower};
	// 两个独立消费者同时持有同一组冻结事件；旧 Removed 必须复核最新已提交需求。
	for (const auto& Events : {Added, Removed, Revived})
	{
		Module->QueueSimulationDemandEvents(Events);
		Module->ResidencyManager->QueueDemandEvents(Events);
	}
	const auto FactResources = Module->Scheduler->GetRuntimeResources();
	const auto FactState = FactResources->Snapshot();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResidentBlocker;
	const auto BlockerConsumer = FVoxelGenerationConsumerKey::Section(Module->Epoch, FGuid::NewGuid(), 1, Lower, 1);
	if (!TestTrue(TEXT("模拟控制表容量阻塞来自实际资源池"), FactResources->Acquire(BlockerConsumer,
		EVoxelTerrainResourcePool::Resident, FactState.Limits.ResidentBytes - FactState.Limits.OverlapBytes -
			FactState.Used[int32(EVoxelTerrainResourcePool::Resident)], ResidentBlocker))) return false;
	if (!TestTrue(TEXT("容量不足保留事件续行，不提前取得Simulation pin"), Pump([&]()
	{
		return Module->Runtime->FindSection(Lower)->bSimulationCapacityBlocked;
	}))) return false;
	TestFalse(TEXT("控制表授予失败未修改已激活集合"), Module->IsSimulationResident(Lower));
	TestFalse(TEXT("控制表授予失败未取得稳定pin"), Module->Runtime->FindSection(Lower)->bSimulationPinned);
	ResidentBlocker.Reset();
	if (!TestTrue(TEXT("Navigation exports actual geometry after Simulation activation"), Pump([&]()
	{
		return FindSurface(Lower) && !FindSurface(Lower)->GetSurface().Indices.IsEmpty() &&
			FindSurface(Distant) && !FindSurface(Distant)->GetSurface().Indices.IsEmpty();
	}))) return false;
	TestTrue(TEXT("Lower section is simulation resident"), Module->IsSimulationResident(Lower));
	TestFalse(TEXT("连续提交的模拟增减事件已按各自链排空"), Module->SimulationDemandEvents.IsValid());
	TestFalse(TEXT("Upper clearance section remains data-only"), Module->IsSimulationResident(Above));
	TestEqual(TEXT("One support face is exported across the section boundary"), FindSurface(Lower)->GetSurface().Indices.Num(), 6);

	Interest->Exact.Remove(Above);
	Module->ResidencyManager->RequestRetirement(Above);
	const uint64 PreviousFrame = GFrameCounter;
	GFrameCounter += 15;
	Module->ResidencyManager->Tick(Interest->Exact, 2, 0.);
	GFrameCounter = PreviousFrame;
	TestNotNull(TEXT("Nav自身的上层消费者保留已经退出视觉/Source的数据"), Module->Runtime->FindSection(Above));
	TestFalse(TEXT("上层稳定pin不能由外部Runtime移除绕过"), Module->Runtime->RemoveSection(Above));
	TestEqual(TEXT("上层需求退出仍保留当前净空输入及表面"), FindSurface(Lower)->GetSurface().Indices.Num(), 6);
	TestEqual(TEXT("独立导航岛保留自己的表面"), FindSurface(Distant)->GetSurface().Indices.Num(), 6);
	TestTrue(TEXT("The lower data and Simulation owner remain resident"), Module->Runtime->FindSection(Lower) && Module->IsSimulationResident(Lower));
	FVoxelCellEdit Ceiling;
	Ceiling.Position = FIntVector(4, 4, 19);
	Ceiling.Value = FVoxelBlockState(SupportType, 0);
	FVoxelPreparedEdit Prepared;
	FVoxelEditBatch Batch;
	if (!TestTrue(TEXT("Overhead edit commits through the runtime"), Module->Runtime->PrepareEdit({Ceiling}, {}, Prepared, Error) &&
		Module->Runtime->CommitPreparedEdit(MoveTemp(Prepared), Batch, Error))) return false;
	Module->PublishProjectEdit(Batch);
	TestTrue(TEXT("Committed overhead edit immediately invalidates dependent navigation"), FindSurface(Lower)->GetSurface().Indices.IsEmpty());
	TestEqual(TEXT("Committed overhead edit leaves distant navigation intact"), FindSurface(Distant)->GetSurface().Indices.Num(), 6);

	Module->NavigationPresenter->Tick();
	const FIntVector SharedRegionSection(1, 0, 0);
	TestTrue(TEXT("同Region第二个模拟Section及净空输入发布"), PublishSection(SharedRegionSection, 0) &&
		PublishSection(SharedRegionSection + FIntVector(0, 0, 1), -1));
	auto& SharedDemand = Interest->Exact.Add(SharedRegionSection);
	SharedDemand.bSimulation = true;
	SharedDemand.bCollision = true;
	SharedDemand.ServicePriority = {EVoxelTerrainServiceBand::P0, 0.0, 0.0, 0, FGuid::NewGuid()};
	Inputs->RuntimeSections.Add(SharedRegionSection);
	Module->WakeSimulationSection(SharedRegionSection);
	if (!TestTrue(TEXT("同Region第二个Section实际取得模拟驻留"), Pump([&]()
	{
		return Module->IsSimulationResident(SharedRegionSection);
	}))) return false;
	const auto* LowerData = Module->Runtime->FindSection(Lower);
	if (!TestTrue(TEXT("活动模拟独立持有稳定 Runtime pin"), LowerData && LowerData->bSimulationPinned &&
		LowerData->GetPinCount() > 0)) return false;
	auto* Region = Cast<UVoxelNavigationTestSceneRegion>(Module->GetSceneRegion(Lower, false));
	if (!TestNotNull(TEXT("模拟区域保留实际状态 owner"), Region)) return false;
	const int32 PinsBefore = LowerData->GetPinCount();
	TWeakObjectPtr<AVoxelNavigationBounds> RetainedBounds;
	for (TActorIterator<AVoxelNavigationBounds> Iterator(World); Iterator; ++Iterator)
	{
		if (Iterator->GetComponentsBoundingBox().IsInsideOrOn(FVector(8, 8, 8) * Module->BlockSize())) RetainedBounds = *Iterator;
	}
	if (!TestTrue(TEXT("共享导航区域持有真实局部边界"), RetainedBounds.IsValid())) return false;
	const FBox RetainedBox = RetainedBounds->GetComponentsBoundingBox();
	AddExpectedError(TEXT("Simulation region capture failed; residency and pin retained"),
		EAutomationExpectedErrorFlags::Contains, 1);
	Region->bRejectProjectCapture = true;
	Interest->Exact.FindChecked(Lower).bSimulation = false;
	const auto Retiring = MakeShared<FVoxelNavigationSimulationDemandEvents, ESPMode::ThreadSafe>();
	Retiring->Delta.RemovedSimulation = {Lower};
	Module->QueueSimulationDemandEvents(Retiring);
	if (!TestTrue(TEXT("模拟退出按事件推进到明确的区域失败"), Pump([&]() { return Region->bSceneFailed; }))) return false;
	TestTrue(TEXT("局部保存失败不停止独立区域服务"), Module->WorldState == EVoxelWorldState::Running);
	TestTrue(TEXT("保存失败保持模拟驻留，不提前撤销状态 owner"), Module->IsSimulationResident(Lower));
	TestTrue(TEXT("保存失败保持区域Active、导航组件和原稳定pin"), Region->HasActiveSections() &&
		FindSurface(Lower) && LowerData->bSimulationPinned && LowerData->GetPinCount() == PinsBefore);
	TestTrue(TEXT("兴趣源退出不能撤销实际导航边界"), RetainedBounds.IsValid() &&
		RetainedBounds->GetComponentsBoundingBox() == RetainedBox);
	TestFalse(TEXT("Source离开后保存失败仍不能回收模拟输入"), Module->Runtime->RemoveSection(Lower));
	Interest->Exact.FindChecked(SharedRegionSection).bSimulation = false;
	Module->WakeSimulationSection(SharedRegionSection);
	if (!TestTrue(TEXT("区域失败保留后续退出票据"), Pump([&]()
	{
		return Module->Runtime->FindSection(SharedRegionSection)->SimulationWaitingRegion == Region;
	}))) return false;
	TestTrue(TEXT("同区域等待票据不提前归还稳定pin"), Module->IsSimulationResident(SharedRegionSection) &&
		Module->Runtime->FindSection(SharedRegionSection)->bSimulationPinned);
	TestTrue(TEXT("未激活失败票据输入可以发布"), PublishSection(RemovedPending, 0));
	Interest->Exact.Add(RemovedPending).bSimulation = true;
	Module->WakeSimulationSection(RemovedPending);
	if (!TestTrue(TEXT("未激活需求同样保留在失败区域局部链"), Pump([&]()
	{
		return Module->Runtime->FindSection(RemovedPending)->SimulationWaitingRegion == Region;
	}))) return false;
	Interest->Exact.Remove(RemovedPending);
	TestTrue(TEXT("外部移除解除失败区域票据"), Module->Runtime->RemoveSection(RemovedPending));
	Region->bRejectProjectCapture = false;
	Region->bAttemptWorldStopOnCapture = true;
	TestTrue(TEXT("明确恢复入口重新唤醒保留的owner"), Module->RetrySimulationSection(Lower));
	if (!TestTrue(TEXT("一次区域恢复继续全部保留票据"), Pump([&]()
	{
		return !Module->IsSimulationResident(Lower) && !Module->IsSimulationResident(SharedRegionSection);
	}))) return false;
	TestTrue(TEXT("项目保存回调不能重入销毁正在使用的世界owner"), Region->bWorldStopRejected &&
		Module->WorldState == EVoxelWorldState::Running);
	TestFalse(TEXT("Removing only Simulation retires the scene residency immediately"), Module->IsSimulationResident(Lower));
	TestTrue(TEXT("保存与休眠成功后才归还模拟pin"), !LowerData->bSimulationPinned &&
		LowerData->GetPinCount() == PinsBefore - 2);
	TestFalse(TEXT("最后实际Nav消费者退出才撤销共享固定边界"), RetainedBounds.IsValid());
	TestTrue(TEXT("最后上层Nav消费者退出后归还真实pin"), Module->Runtime->FindSection(Above)->GetPinCount() == 0);
	TestNull(TEXT("Removing only Simulation destroys its navigation component"), FindSurface(Lower));
	TestNotNull(TEXT("Simulation removal retains demanded exact data"), Module->Runtime->FindSection(Lower));
	TestTrue(TEXT("Already queued navigation work drains"), Pump([&]()
	{
		return Module->Scheduler->GetDiagnostics().Pending == 0 && Module->Scheduler->ActiveCount() == 0;
	}));
	TestNull(TEXT("A late result cannot resurrect retired navigation"), FindSurface(Lower));
	TestEqual(TEXT("An independent Simulation island keeps its exported face"), FindSurface(Distant)->GetSurface().Indices.Num(), 6);
	const FIntVector NavigationOnly(-3, 0, 0);
	if (!TestTrue(TEXT("无Source的实际Nav消费者取得上下层和物理控制接纳"), PublishSection(NavigationOnly, 0) &&
		PublishSection(NavigationOnly + FIntVector(0, 0, 1), -1) && Module->NavigationPresenter->ActivateSection(NavigationOnly))) return false;
	if (!TestTrue(TEXT("无Source的首次Nav需求经真实物理提交后才导出表面"), Pump([&]()
	{
		return Module->IsCollisionReady(NavigationOnly) && FindSurface(NavigationOnly) &&
			!FindSurface(NavigationOnly)->GetSurface().Indices.IsEmpty();
	}))) return false;
	TestFalse(TEXT("独立Nav物理需求不冒充Simulation"), Module->IsSimulationResident(NavigationOnly));
	Module->NavigationPresenter->RetireSection(NavigationOnly);
	TestFalse(TEXT("未被读取的独立Nav需求可立即归还"), Module->IsNavigationResident(NavigationOnly));
	Module->TerrainFrameContext.Begin(Module->Epoch, ++TestFrame, FPlatformTime::Seconds(), {});
	Module->CollisionPresenter->Tick(Interest->Exact, TestFrame);
	Module->TerrainFrameContext.End(FPlatformTime::Seconds());
	TestTrue(TEXT("实际物理退役期间重新出现的独立Nav需求可接纳"), Module->NavigationPresenter->ActivateSection(NavigationOnly));
	if (!TestTrue(TEXT("旧物理注销完成不能吞掉无Source的新版Nav需求"), Pump([&]()
	{
		return Module->IsCollisionReady(NavigationOnly) && FindSurface(NavigationOnly) &&
			!FindSurface(NavigationOnly)->GetSurface().Indices.IsEmpty();
	}))) return false;
	Module->NavigationPresenter->RetireSection(NavigationOnly);
	FVoxelNavigationSurfaceReadLease Reader;
	if (!TestTrue(TEXT("真实已发布表面可以取得独立读取者"), FindSurface(Distant)->AcquireSurfaceRead(Reader))) return false;
	FVoxelPhysicsReceipt PreviousPhysics;
	if (!TestTrue(TEXT("读取者持有实际已提交物理体"), Module->CaptureCollisionReceipt(Distant, PreviousPhysics))) return false;
	const int32 DistantPins = Module->Runtime->FindSection(Distant)->GetPinCount();
	Interest->Exact.Remove(Distant);
	Inputs->RuntimeSections.Remove(Distant);
	Module->WakeSimulationSection(Distant);
	if (!TestTrue(TEXT("Source与Simulation退出后读取消费者仍持有Nav"), Pump([&]()
	{
		return !Module->IsSimulationResident(Distant) && Module->IsNavigationResident(Distant);
	}))) return false;
	TestTrue(TEXT("Source退出保留冻结表面和真实碰撞输入"), Reader.IsCurrent() && FindSurface(Distant) &&
		Module->IsCollisionReady(Distant));
	TestEqual(TEXT("Source退出只归还Simulation pin，Nav下层pin仍持有"),
		Module->Runtime->FindSection(Distant)->GetPinCount(), DistantPins - 1);
	TestTrue(TEXT("Nav上层pin独立于当前Source"),
		Module->Runtime->FindSection(Distant + FIntVector(0, 0, 1))->GetPinCount() > 0);
	FVoxelNavigationSurfaceReadLease LateReader;
	TestFalse(TEXT("等待最后读取者退役时关闭新读取接纳"), FindSurface(Distant)->AcquireSurfaceRead(LateReader));
	TestFalse(TEXT("主动停止不能先销毁仍被实际读取者使用的输入"), Module->StopWorld(true, Error));
	TestTrue(TEXT("停止等待不提前更换世界代或破坏原读取"), Module->Epoch == 11 && Reader.IsCurrent());
	FVoxelCellEdit DistantEdit;
	DistantEdit.Position = Distant * 16 + FIntVector(5, 4, 0);
	DistantEdit.Value = FVoxelBlockState(SupportType, 0);
	if (!Module->Runtime->PrepareEdit({DistantEdit}, {}, Prepared, Error) ||
		!Module->Runtime->CommitPreparedEdit(MoveTemp(Prepared), Batch, Error)) return false;
	Module->PublishProjectEdit(Batch);
	TestFalse(TEXT("实际内容换代即时撤销冻结表面读取证明"), Reader.IsCurrent());
	if (!TestTrue(TEXT("没有Source键的Nav仍通过实际Body局部队列完成物理换代"), Pump([&]()
	{
		FVoxelPhysicsReceipt CurrentPhysics;
		return Module->CaptureCollisionReceipt(Distant, CurrentPhysics) &&
			CurrentPhysics.BodyGeneration != PreviousPhysics.BodyGeneration;
	}))) return false;
	// 真实工作线程归还只改变嵌入的消费者计数，随后既有 GT 退役队列撤销需求。
	auto ReleaseReader = UE::Tasks::Launch(UE_SOURCE_LOCATION, [Lease = MoveTemp(Reader)]() mutable
	{
		Lease.Reset();
	});
	ReleaseReader.Wait();
	if (!TestTrue(TEXT("工作线程最后归还由局部GT队列完成实际Nav退役"), Pump([&]()
	{
		return !Module->IsNavigationResident(Distant) && !FindSurface(Distant);
	}))) return false;
	TestEqual(TEXT("最后实际读取退出后归还上层pin"),
		Module->Runtime->FindSection(Distant + FIntVector(0, 0, 1))->GetPinCount(), 0);
	TestEqual(TEXT("最后实际读取退出后归还下层Nav pin"), Module->Runtime->FindSection(Distant)->GetPinCount(), 0);

	// 持久 Scene 退役使用同一个真实 Runtime/Physics/Nav 夹具和实际 SaveGame Worker 文件代。
	FSaveGameStorage SceneStorage(0, FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("VoxelSceneSaveCut")));
	const FGuid SceneSaveId = FGuid::NewGuid();
	const FGuid SceneSaveAsId = FGuid::NewGuid();
	FSaveManifest SceneManifest;
	SceneManifest.SaveId = SceneSaveId;
	SceneManifest.DisplayName = TEXT("Automation scene durable cut");
	if (!TestTrue(TEXT("Scene回归创建独立的实际存档槽"), SceneStorage.CreateWorldDirectory(SceneSaveId) &&
		SceneStorage.WriteManifestAtomic(SceneSaveId, SceneManifest))) return false;
	Module->Manifest.WorldId = FGuid::NewGuid();
	Module->Manifest.RegistryHash = Module->Registry.GetSnapshot()->Hash;
	Module->Manifest.RecipeHash = VoxelTest::MakeGenerationConfig()->Recipe->RecipeHash;
	Module->SetPersistenceEnabled(true);
	auto& RestoredDemand = Interest->Exact.Add(Distant);
	RestoredDemand.bSimulation = true;
	RestoredDemand.bCollision = true;
	RestoredDemand.ServicePriority = {EVoxelTerrainServiceBand::P0, 0.0, 0.0, 0, FGuid::NewGuid()};
	Inputs->RuntimeSections.Add(Distant);
	Module->WakeSimulationSection(Distant);
	if (!TestTrue(TEXT("持久世界经原激活链重新取得Simulation"), Pump([&]()
	{
		return Module->IsSimulationResident(Distant);
	}))) return false;
	const auto SceneIdentity = Module->Runtime->FindSection(Distant)->GetResourceIdentity();
	const int32 ScenePins = Module->Runtime->FindSection(Distant)->GetPinCount();
	Interest->Exact.FindChecked(Distant).bSimulation = false;
	Module->WakeSimulationSection(Distant);
	if (!TestTrue(TEXT("休眠后等待真实文件代，稳定Simulation pin仍被原owner持有"), Pump([&]()
	{
		const auto* Section = Module->Runtime->FindSection(Distant);
		return Section && Section->SimulationSaveRegion && !Module->IsSimulationResident(Distant);
	}))) return false;
	TestEqual(TEXT("内存Scene编码不能归还任何原输入pin"), Module->Runtime->FindSection(Distant)->GetPinCount(), ScenePins);
	TestTrue(TEXT("保存等待不冒充业务活动集合"), !Module->GetSimulationSections().Contains(Distant));
	TestFalse(TEXT("保存等待禁止正常关闭世界"), Module->StopWorld(false, Error));
	TestFalse(TEXT("保存等待不能绕过Runtime pin被回收"), Module->Runtime->RemoveSection(Distant));

	{
		auto* Producer = NewObject<UVoxelNavigationTestSceneRegion>(Module);
		Producer->Initialize(Module, FIntVector(90, 90, 0));
		TArray<uint8> EmptySceneBytes;
		if (!TestTrue(TEXT("独立 Scene producer 通过真实空存档恢复后接纳保存"),
			FVoxelSceneColumnCodec::Encode({}, FParameter(), EmptySceneBytes, Error) && Producer->RestoreActors(EmptySceneBytes, Error))) return false;
		FVoxelModuleSaveData ActorData;
		ActorData.Seed = 91;
		for (int32 Index = 0; Index < 130; ++Index) Producer->AddPendingCaptureActor(FGuid::NewGuid(), FParameter(ActorData));
		FVoxelSceneColumnCaptureCursor Cursor;
		if (!TestTrue(TEXT("原 Scene producer 元数据接纳不捕获 Actor 行"),
			Producer->AdvanceSavePayloads(Cursor, Error) == EVoxelSceneCaptureStep::Pending && Cursor.Capture && Cursor.Capture->Actors.IsEmpty())) return false;
		for (int32 Step = 0; Step < 32; ++Step)
		{
			const int32 PreviousCount = Cursor.Capture->Actors.Num();
			const auto Result = Producer->AdvanceSavePayloads(Cursor, Error);
			if (!TestTrue(TEXT("每步捕获至多一个实际反射 Actor payload"), Result == EVoxelSceneCaptureStep::Pending &&
				Cursor.Capture->Actors.Num() <= PreviousCount + 1)) return false;
		}
		TestTrue(TEXT("32步没有整批同步完成130个Actor"), Cursor.Capture->Actors.Num() < 130);
		EVoxelSceneCaptureStep State = EVoxelSceneCaptureStep::Pending;
		for (int32 Step = 0; Step < 256 && State == EVoxelSceneCaptureStep::Pending; ++Step)
			State = Producer->AdvanceSavePayloads(Cursor, Error);
		TestTrue(TEXT("持久游标最终捕获全部原 Pending Actor 数据"), State == EVoxelSceneCaptureStep::Complete && Cursor.Capture->Actors.Num() == 130);
		FVoxelSceneColumnCaptureCursor Revoked;
		if (Producer->AdvanceSavePayloads(Revoked, Error) != EVoxelSceneCaptureStep::Pending) return false;
		Producer->OnSectionActivated(FIntVector(91, 91, 0));
		TestTrue(TEXT("相同对象中内容版本改变会拒绝旧保存producer"), Producer->AdvanceSavePayloads(Revoked, Error) == EVoxelSceneCaptureStep::Failed);
		Producer->Shutdown();
	}

	auto CaptureScenePlan = [&](FSaveGenerationPlan& Plan, const bool bWorkerResolvesTarget = false,
		const FGuid* InTargetSaveId = nullptr)
	{
		Module->OnBeforeSaveData();
		TestTrue(TEXT("OnBeforeSaveData 只接纳固定切点游标而不遍历 Scene"), Module->SceneSaveBuilder.IsSet() && !Module->CapturedSceneSave);
		if (!Pump([&]()
		{
			const auto State = Module->GetSaveCaptureState(Error);
			return State == EVoxelSceneCaptureStep::Complete || State == EVoxelSceneCaptureStep::Failed;
		})) return false;
		FVoxelModuleSaveCapture Capture;
		if (!Module->CopySaveCapture(Capture, Error)) { AddError(Error); return false; }
		Plan.SaveId = InTargetSaveId ? *InTargetSaveId : SceneSaveId;
		Plan.StorageRoot = SceneStorage.GetRootDir();
		Plan.bResolveManifestOnWorker = bWorkerResolvesTarget;
		if (bWorkerResolvesTarget)
		{
			if (!Module->BindSaveCommitOwner(Plan.SaveId, Plan.StorageRoot, Error)) return false;
		}
		else if (!SceneStorage.ReadManifest(Plan.SaveId, Plan.Manifest) || !Module->BindSaveCommitTarget(Plan.SaveId,
			Plan.Manifest.CurrentGeneration + 1, SceneStorage.GetGenerationDir(Plan.SaveId, Plan.Manifest.CurrentGeneration + 1), Error)) return false;
		Plan.Voxel = MoveTemp(Capture);
		return true;
	};
	{
		FSaveGenerationPlan Unwritten;
		if (!TestTrue(TEXT("实际Module冻结Scene与Voxel同事务切点"), CaptureScenePlan(Unwritten))) return false;
		Module->OnAfterSaveData(true);
		TestFalse(TEXT("单独Success布尔值不能伪造磁盘提交回执"), Module->bLatestSceneSaveSucceeded);
		TestTrue(TEXT("未写入的伪完成不能将原Voxel Journal标记落盘"), Module->Runtime->HasUnpersistedChanges());
		if (!TestTrue(TEXT("无真实Worker回执的等待者保留失败状态与pin"), Pump([&]()
		{
			return Module->GetSceneRegion(Distant, false)->bSceneFailed;
		}))) return false;
		TestEqual(TEXT("伪完成没有归还稳定输入pin"), Module->Runtime->FindSection(Distant)->GetPinCount(), ScenePins);
		if (!TestTrue(TEXT("显式恢复启动新的实际保存尝试"), Module->RetrySimulationSection(Distant))) return false;

		FSaveGenerationPlan First;
		if (!TestTrue(TEXT("恢复后重新冻结实际切点，由原Worker解析真实文件代"), CaptureScenePlan(First, true))) return false;
		const FIntVector SceneKey = Module->GetSceneRegion(Distant, false)->GetRegionKey();
		const auto FirstCut = First.Voxel->Scenes->Regions.FindChecked(SceneKey);
		Interest->Exact.FindChecked(Distant).bSimulation = true;
		Module->WakeSimulationSection(Distant);
		if (!TestTrue(TEXT("实际保存运行前的新Simulation需求复用原稳定身份"), Pump([&]()
		{
			return Module->IsSimulationResident(Distant);
		}))) return false;
		TestTrue(TEXT("恢复仍是原Runtime实例"), Module->Runtime->FindSection(Distant)->GetResourceIdentity() == SceneIdentity);
		FSaveGameAsyncExecutor FirstWorker;
		if (!FirstWorker.Start(MoveTemp(First))) return false;
		FSaveOperationResult FirstResult;
		bool bFirstPolled = false;
		if (!TestTrue(TEXT("实际Worker退出后才读取文件代完成"), Pump([&]()
		{
			return bFirstPolled || (bFirstPolled = FirstWorker.Poll(FirstResult, &Module->TerrainFrameContext));
		}, true))) return false;
		if (!TestTrue(FString::Printf(TEXT("实际Scene文件、目录rename和manifest提交成功: code=%d message=%s"),
			int32(FirstResult.Code), *FirstResult.Message.ToString()), bool(FirstResult))) return false;
		Module->OnAfterSaveData(bool(FirstResult));
		TestTrue(TEXT("实际提交回执可在GT应用"), Module->bLatestSceneSaveSucceeded);
		TestTrue(TEXT("已恢复的Simulation不被旧切点释放"), Module->IsSimulationResident(Distant) &&
			Module->Runtime->FindSection(Distant)->bSimulationPinned);
		if (!SceneStorage.ReadManifest(SceneSaveId, SceneManifest)) return false;
		TArray<uint8> SceneDiskBytes;
		TArray<FVoxelSavedSceneActor> SceneDiskActors;
		FParameter SceneDiskProject;
		TestTrue(TEXT("已提交文件代中的Scene文件可以实际读回和解码"), SceneStorage.ReadBinary(FPaths::Combine(
			SceneStorage.GetGenerationDir(SceneSaveId, SceneManifest.CurrentGeneration),
			FVoxelSceneColumnCodec::RelativePath(FIntPoint(SceneKey.X, SceneKey.Y))), SceneDiskBytes) &&
			FVoxelSceneColumnCodec::Decode(SceneDiskBytes, SceneDiskActors, SceneDiskProject, Error));

		// 另存时 Journal 已干净，旧代中的非驻留编辑仍必须由真实基线文件复制保留。
		TArray<uint8> BaselineEdit;
		if (!TestTrue(TEXT("原实际提交代含有可读的Voxel编辑基线"),
			Module->RegionStore.ReadSection(Distant, BaselineEdit, Error) == EVoxelRegionRead::Loaded && !BaselineEdit.IsEmpty())) return false;
		TestFalse(TEXT("另存前原Journal不再有未提交编辑"), Module->Runtime->HasUnpersistedChanges());
		FSaveManifest SaveAsManifest;
		SaveAsManifest.SaveId = SceneSaveAsId;
		if (!SceneStorage.CreateWorldDirectory(SceneSaveAsId) || !SceneStorage.WriteManifestAtomic(SceneSaveAsId, SaveAsManifest)) return false;
		FSaveGenerationPlan SaveAs;
		if (!CaptureScenePlan(SaveAs, true, &SceneSaveAsId)) return false;
		FSaveGameAsyncExecutor SaveAsWorker;
		if (!SaveAsWorker.Start(MoveTemp(SaveAs))) return false;
		FSaveOperationResult SaveAsResult;
		bool bSaveAsPolled = false;
		if (!Pump([&]() { return bSaveAsPolled || (bSaveAsPolled = SaveAsWorker.Poll(SaveAsResult, &Module->TerrainFrameContext)); }, true) ||
			!TestTrue(FString::Printf(TEXT("原真实Worker完成另存到空目标槽: code=%d message=%s"),
				int32(SaveAsResult.Code), *SaveAsResult.Message.ToString()), bool(SaveAsResult))) return false;
		Module->OnAfterSaveData(bool(SaveAsResult));
		TArray<uint8> SaveAsEdit;
		TestTrue(TEXT("干净Journal的另存仍保留源槽的实际编辑正文"),
			Module->RegionStore.ReadSection(Distant, SaveAsEdit, Error) == EVoxelRegionRead::Loaded && SaveAsEdit == BaselineEdit);
		TestTrue(TEXT("另存GT回执采用实际目标槽而非旧Active基线"),
			SceneStorage.ReadManifest(SceneSaveAsId, SaveAsManifest) &&
			FPaths::IsSamePath(Module->RegionStore.GetSourceDirectory(), SceneStorage.GetGenerationDir(SceneSaveAsId, SaveAsManifest.CurrentGeneration)));

		Interest->Exact.FindChecked(Distant).bSimulation = false;
		Module->WakeSimulationSection(Distant);
		if (!Pump([&]() { return Module->Runtime->FindSection(Distant)->SimulationSaveRegion != nullptr; })) return false;
		TestTrue(TEXT("第二次休眠要求新内容版本，旧实际文件代不能释放"),
			Module->Runtime->FindSection(Distant)->SimulationSaveRevision > FirstCut.Revision &&
			Module->Runtime->FindSection(Distant)->bSimulationPinned);
		TestFalse(TEXT("Voxel已落盘后仍由Scene保存义务阻止正常关闭"), Module->StopWorld(false, Error));
		TestTrue(TEXT("正常关闭拒绝来自实际Scene等待者"), Error.Contains(TEXT("scene owners")));
		FSaveGenerationPlan StalePlan;
		if (!CaptureScenePlan(StalePlan)) return false;
		++StalePlan.Manifest.CurrentGeneration;
		FSaveGameAsyncExecutor FailedWorker;
		if (!FailedWorker.Start(MoveTemp(StalePlan))) return false;
		FSaveOperationResult FailedResult;
		bool bFailedPolled = false;
		if (!Pump([&]() { return bFailedPolled || (bFailedPolled = FailedWorker.Poll(FailedResult, &Module->TerrainFrameContext)); }, true)) return false;
		TestFalse(TEXT("实际存档代冲突返回失败"), bool(FailedResult));
		Module->OnAfterSaveData(bool(FailedResult));
		if (!Pump([&]() { return Module->GetSceneRegion(Distant, false)->bSceneFailed; })) return false;
		TestTrue(TEXT("实际保存失败继续保留同一Runtime实例和pin"),
			Module->Runtime->FindSection(Distant)->GetResourceIdentity() == SceneIdentity &&
			Module->Runtime->FindSection(Distant)->bSimulationPinned);
		if (!Module->RetrySimulationSection(Distant)) return false;
		FSaveGenerationPlan Final;
		if (!CaptureScenePlan(Final)) return false;
		FSaveGameAsyncExecutor FinalWorker;
		if (!FinalWorker.Start(MoveTemp(Final))) return false;
		FSaveOperationResult FinalResult;
		bool bFinalPolled = false;
		if (!Pump([&]() { return bFinalPolled || (bFinalPolled = FinalWorker.Poll(FinalResult, &Module->TerrainFrameContext)); }, true) ||
			!TestTrue(FString::Printf(TEXT("重试实际提交新的保存代: code=%d message=%s"),
				int32(FinalResult.Code), *FinalResult.Message.ToString()), bool(FinalResult))) return false;
		Module->OnAfterSaveData(bool(FinalResult));
		if (!TestTrue(TEXT("匹配的真实保存回执经局部链完成pin与区域释放"), Pump([&]()
		{
			return !Module->Runtime->FindSection(Distant)->bSimulationPinned && !Module->GetSceneRegion(Distant, false);
		}))) return false;
		Interest->Exact.FindChecked(Distant).bSimulation = true;
		Module->WakeSimulationSection(Distant);
		if (!Pump([&]() { return Module->IsSimulationResident(Distant); })) return false;
		Interest->Exact.FindChecked(Distant).bSimulation = false;
		Module->WakeSimulationSection(Distant);
		if (!Pump([&]() { return Module->Runtime->FindSection(Distant)->SimulationSaveRegion != nullptr; })) return false;
		const auto* Recreated = Module->Runtime->FindSection(Distant);
		TestTrue(TEXT("同坐标新Region不能借用旧实例的实际提交回执"),
			Recreated->SimulationSaveRegionId != FirstCut.RegionInstanceId && Recreated->bSimulationPinned);
	}
	if (!Pump([&]() { return Module->StopWorld(true, Error); })) return false;
	Module->RegionStore.Reset();
	const double GenerationRetireDeadline = FPlatformTime::Seconds() + 5.0;
	while ((FSaveGameGenerationReadLease::GetPinCount(SceneStorage.GetWorldDir(SceneSaveId)) != 0 ||
		FSaveGameGenerationReadLease::GetPinCount(SceneStorage.GetWorldDir(SceneSaveAsId)) != 0) &&
		FPlatformTime::Seconds() < GenerationRetireDeadline) FPlatformProcess::SleepNoStats(0.001f);
	TestTrue(TEXT("独立测试存档在所有owner归还后清理"), SceneStorage.DeleteWorldDirectory(SceneSaveId));
	TestTrue(TEXT("另存测试槽在所有源代租约归还后清理"), SceneStorage.DeleteWorldDirectory(SceneSaveAsId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNavigationReadIdentityTest,
	"WHFramework.Voxel.Navigation.ReadLeaseVersionAndMoveOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNavigationReadIdentityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	struct FInputConsumer final : IVoxelNavigationInvalidationConsumer
	{
		int32 Revocations = 0;
		virtual void OnNavigationInputRevoked() override
		{
			++Revocations;
		}
	};
	const auto Consumers = MakeShared<FVoxelNavigationSectionConsumers, ESPMode::ThreadSafe>();
	const auto First = MakeShared<FVoxelNavigationSurface, ESPMode::ThreadSafe>();
	First->Access = MakeShared<FVoxelNavigationSurfaceAccess, ESPMode::ThreadSafe>(Consumers);
	FVoxelNavigationSurfaceReadLease Lease;
	TestFalse(TEXT("未实际发布不能接纳读取"), Lease.Acquire(First));
	First->Access->Publish();
	const auto Consumer = MakeShared<FInputConsumer, ESPMode::ThreadSafe>();
	FVoxelNavigationInvalidationObserver Observer;
	TestTrue(TEXT("真实发布输入可以登记预分配的撤销反向边"), First->Access->Observe(Observer, Consumer));
	TestTrue(TEXT("实际发布可接纳唯一读取引用"), Lease.Acquire(First));
	TestEqual(TEXT("取得读取增加实际消费者计数"), Consumers->Num(), 1);
	TestFalse(TEXT("同一句柄不能重复取得引用"), Lease.Acquire(First));
	FVoxelNavigationSurfaceReadLease Moved(MoveTemp(Lease));
	TestNull(TEXT("移动后旧句柄不再拥有正文"), Lease.Get());
	Lease.Reset();
	TestEqual(TEXT("旧句柄归还不释放新句柄引用"), Consumers->Num(), 1);
	First->Access->Revoke();
	TestEqual(TEXT("真实表面撤销立即通知实际使用者"), Consumer->Revocations, 1);
	First->Access->Revoke();
	TestEqual(TEXT("重复撤销不重复派发同一反向边"), Consumer->Revocations, 1);
	First->Access->Publish();
	TestFalse(TEXT("同令牌重新Apply不能复活已撤销的tile输入"), Moved.IsCurrent());
	const auto Second = MakeShared<FVoxelNavigationSurface, ESPMode::ThreadSafe>();
	Second->Access = MakeShared<FVoxelNavigationSurfaceAccess, ESPMode::ThreadSafe>(Consumers);
	Second->Access->Publish();
	TestTrue(TEXT("同一个已脱链节点可订阅独立新版输入"), Second->Access->Observe(Observer, Consumer));
	Observer.Detach();
	Second->Access->Revoke();
	TestEqual(TEXT("提前归还的使用者不再接收旧输入撤销"), Consumer->Revocations, 1);
	// 后续读取验证使用第三次实际发布，已撤销的Second令牌不能复活。
	Second->Access = MakeShared<FVoxelNavigationSurfaceAccess, ESPMode::ThreadSafe>(Consumers);
	Second->Access->Publish();
	TestFalse(TEXT("新版表面不能让旧读取借用新版状态"), Moved.IsCurrent());
	Consumers->SetAdmissionOpen(false);
	TestFalse(TEXT("Source退役关闭新读取接纳"), Lease.Acquire(Second));
	TestTrue(TEXT("已有真实路径引用允许同Section跨版本续读而不开放普通接纳"), Lease.AcquireContinuation(Second, Moved));
	TestEqual(TEXT("跨版本续读保留旧新重叠的实际引用"), Consumers->Num(), 2);
	Lease.Reset();
	const auto Foreign = MakeShared<FVoxelNavigationSurface, ESPMode::ThreadSafe>();
	Foreign->Access = MakeShared<FVoxelNavigationSurfaceAccess, ESPMode::ThreadSafe>(
		MakeShared<FVoxelNavigationSectionConsumers, ESPMode::ThreadSafe>());
	Foreign->Access->Publish();
	TestFalse(TEXT("续读权限不能转借其他Section消费者控制"), Lease.AcquireContinuation(Foreign, Moved));
	Consumers->SetAdmissionOpen(true);
	TestTrue(TEXT("重新出现的Source使用独立新版读取"), Lease.Acquire(Second));
	TestEqual(TEXT("两个不同版本引用共享Section的实际退役计数"), Consumers->Num(), 2);
	Moved.Reset();
	TestEqual(TEXT("旧版本归还只释放自身读取"), Consumers->Num(), 1);
	Consumers->CloseWorld();
	Consumers->SetAdmissionOpen(true);
	TestFalse(TEXT("旧世界关闭不能由Source重新接纳复活"), Lease.IsCurrent());
	TestFalse(TEXT("旧世界拒绝迟到读取"), Moved.Acquire(Second));
	TestFalse(TEXT("已有路径引用不能跨越已关闭世界续读"), Moved.AcquireContinuation(Second, Lease));
	Lease.Reset();
	TestEqual(TEXT("各版本实际归还后计数排空"), Consumers->Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNavigationDoorClearanceTest,
	"WHFramework.Voxel.Navigation.ObjectDoorClearanceTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNavigationDoorClearanceTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelRegistrySnapshot Registry;
	Registry.Definitions.SetNum(3);
	Registry.Definitions[1].TypeId = 1;
	Registry.Definitions[1].bSolid = true;
	Registry.Definitions[2].TypeId = 2;
	Registry.Definitions[2].bSolid = true;
	Registry.Definitions[2].Shape = EVoxelShapeKind::ObjectPart;
	FVoxelShapeRegistry Shapes;
	Shapes.BuildDefaults();
	FVoxelSectionSnapshot Section;
	Section.Blocks.Init(0, 4096);
	Section.Blocks[4 + 16 * 4] = FVoxelBlockState(1, 0).Pack();
	const int32 DoorCell = 4 + 16 * (4 + 16);
	Section.Blocks[DoorCell] = FVoxelBlockState(2, 0).Pack();
	FVoxelNavigationSurface Surface;
	TestTrue(TEXT("Closed-door geometry builds"), FVoxelNavigationSurfaceBuilder::Build(Section, nullptr, Registry, Shapes, 25., 175., Surface));
	TestTrue(TEXT("Closed object door removes floor clearance"), Surface.Indices.IsEmpty());
	Section.Blocks[DoorCell] = FVoxelBlockState(2, VoxelState::OpenMask).Pack();
	TestTrue(TEXT("Open-door geometry rebuilds"), FVoxelNavigationSurfaceBuilder::Build(Section, nullptr, Registry, Shapes, 25., 175., Surface));
	TestEqual(TEXT("Opening the actual collision state restores the floor face"), Surface.Indices.Num(), 6);
	Section.Blocks[DoorCell] = FVoxelBlockState(2, 0).Pack();
	FVoxelNavigationSurfaceBuilder::Build(Section, nullptr, Registry, Shapes, 25., 175., Surface);
	TestTrue(TEXT("Closing again replaces rather than appends to old geometry"), Surface.Indices.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNavigationExtendedClearanceTest,
	"WHFramework.Voxel.Navigation.ClearanceAcrossTwoUpperSections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNavigationExtendedClearanceTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelRegistrySnapshot Registry;
	Registry.Definitions.SetNum(2);
	Registry.Definitions[1].TypeId = 1;
	Registry.Definitions[1].bSolid = true;
	FVoxelShapeRegistry Shapes;
	Shapes.BuildDefaults();
	FVoxelSectionSnapshot Lower;
	FVoxelSectionSnapshot Above;
	FVoxelSectionSnapshot SecondAbove;
	Lower.Blocks.Init(0, 4096);
	Above.Blocks.Init(0, 4096);
	SecondAbove.Blocks.Init(0, 4096);
	Lower.Blocks[4 + 16 * (4 + 16 * 15)] = FVoxelBlockState(1, 0).Pack();
	FVoxelNavigationSurface Surface;
	TestTrue(TEXT("缺少第二上层仍只能产生保守表面"),
		FVoxelNavigationSurfaceBuilder::Build(Lower, &Above, Registry, Shapes, 25., 600., Surface));
	TestTrue(TEXT("24格净空不能把未知第二上层当空气"), Surface.Indices.IsEmpty());
	TestTrue(TEXT("完整第二上层参与实际净空采样"),
		FVoxelNavigationSurfaceBuilder::Build(Lower, &Above, Registry, Shapes, 25., 600., Surface,
			nullptr, nullptr, {}, nullptr, &SecondAbove));
	TestEqual(TEXT("跨越两个Section的已知净空可导出地板"), Surface.Indices.Num(), 6);
	SecondAbove.Blocks[4 + 16 * (4 + 16 * 3)] = FVoxelBlockState(1, 0).Pack();
	TestTrue(TEXT("第二上层障碍参与同一真实采样链"),
		FVoxelNavigationSurfaceBuilder::Build(Lower, &Above, Registry, Shapes, 25., 600., Surface,
			nullptr, nullptr, {}, nullptr, &SecondAbove));
	TestTrue(TEXT("第二上层障碍不能遗漏或借用旧净空"), Surface.Indices.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNavigationExportIdentityTest,
	"WHFramework.Voxel.Navigation.OctreeExportFrozenElementIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNavigationExportIdentityTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	auto* Component = NewObject<UVoxelNavigationSurfaceComponent>();
	const auto Surface = MakeShared<FVoxelNavigationSurface, ESPMode::ThreadSafe>();
	Surface->InputVersion.WorldEpoch = 37;
	Surface->InputVersion.SectionToken = 4;
	Component->Apply(Surface);
	const auto First = MakeShared<FNavigationElement>(static_cast<const UObject&>(*Component));
	FNavigationRelevantData FirstData(First);
	Component->GetNavigationData(FirstData);
	TestTrue(TEXT("导出回执属于真实SourceElement和冻结Surface"), Component->CaptureExportedSurface(*First) == Surface);
	const auto Second = MakeShared<FNavigationElement>(static_cast<const UObject&>(*Component));
	FNavigationRelevantData SecondData(Second);
	Component->GetNavigationData(SecondData);
	TestFalse(TEXT("相同UObject和Surface不能复用旧octree元素身份"), Component->CaptureExportedSurface(*First).IsValid());
	TestTrue(TEXT("最新octree元素可捕获同一冻结版本"), Component->CaptureExportedSurface(*Second) == Surface);
	const auto NewSurface = MakeShared<FVoxelNavigationSurface, ESPMode::ThreadSafe>();
	NewSurface->InputVersion = Surface->InputVersion;
	++NewSurface->InputVersion.CollisionBodyGeneration;
	Component->Apply(NewSurface);
	TestFalse(TEXT("新版Body等待实际octree导出，不能借旧导出回执"), Component->CaptureExportedSurface(*Second).IsValid());
	const auto Third = MakeShared<FNavigationElement>(static_cast<const UObject&>(*Component));
	FNavigationRelevantData ThirdData(Third);
	Component->GetNavigationData(ThirdData);
	TestTrue(TEXT("新Body的实际导出保留完整输入版本"), Component->CaptureExportedSurface(*Third) == NewSurface);
	Component->SetWorldLocation(FVector(100., 200., 300.));
	TestFalse(TEXT("变换改变不能借用旧几何导出回执"), Component->CaptureExportedSurface(*Third).IsValid());
	const auto Fourth = MakeShared<FNavigationElement>(static_cast<const UObject&>(*Component));
	FNavigationRelevantData FourthData(Fourth);
	Component->GetNavigationData(FourthData);
	TestTrue(TEXT("实际新变换导出可建立新的冻结捕获"), Component->CaptureExportedSurface(*Fourth) == NewSurface);
	Component->Apply(nullptr);
	TestFalse(TEXT("表面撤回立即撤销旧导出证明"), Component->CaptureExportedSurface(*Fourth).IsValid());
	TestTrue(TEXT("体素geometry和metadata同次导出"), Component->GetGeometryGatheringMode() == ENavDataGatheringMode::Instant);
	return true;
}

#endif
