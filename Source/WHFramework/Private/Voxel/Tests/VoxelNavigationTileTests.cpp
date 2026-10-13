#if WITH_DEV_AUTOMATION_TESTS && WITH_RECAST

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "HAL/PlatformProcess.h"
#include "Main/MainModule.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastQueryFilter.h"
#include "NavMesh/NavMeshPath.h"
#include "Tasks/Task.h"
#include "Voxel/Collision/VoxelCollisionPresenter.h"
#include "Voxel/Collision/VoxelPhysicsRetirement.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Navigation/VoxelNavigationData.h"
#include "Voxel/Navigation/VoxelNavigationSystem.h"
#include "Voxel/Navigation/VoxelNavigationPresenter.h"
#include "Voxel/Navigation/VoxelNavigationRepathControl.h"
#include "Voxel/Navigation/VoxelNavigationSurfaceComponent.h"
#include "Voxel/Streaming/VoxelResidencyManager.h"
#include "Voxel/Tests/VoxelNavigationTestTypes.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Voxels/Data/VoxelData.h"

namespace
{
	struct FVoxelNativeQueryTestGate
	{
		TAtomic<bool> bEntered{false};
		TAtomic<bool> bResume{false};
		TAtomic<int32> CostCalls{0};
	};

	struct FVoxelNativeAsyncQueryResult
	{
		bool bCalled = false;
		bool bPathValidAtDispatch = true;
		uint32 QueryId = 0;
		ENavigationQueryResult::Type Result = ENavigationQueryResult::Error;
		FNavPathSharedPtr Path;
	};

	struct FVoxelNativeRepathEvents
	{
		int32 Invalidated = 0;
		int32 NavigationUpdated = 0;
		int32 GoalUpdated = 0;
		int32 Failed = 0;
	};

	class FVoxelBlockingNativeQueryFilter final : public FRecastQueryFilter
	{
	public:
		FVoxelBlockingNativeQueryFilter(const FRecastQueryFilter& InSource,
			TSharedRef<FVoxelNativeQueryTestGate, ESPMode::ThreadSafe> InGate)
			: FRecastQueryFilter(InSource), Gate(MoveTemp(InGate))
		{
			SetIsVirtual(true);
		}

		virtual INavigationQueryFilterInterface* CreateCopy() const override
		{
			return new FVoxelBlockingNativeQueryFilter(*this);
		}

		virtual FVector GetAdjustedEndLocation(const FVector& InEnd) const override
		{
			return InEnd - FVector(25., 0., 0.);
		}

	protected:
		virtual bool passVirtualFilter(const dtPolyRef InRef, const dtMeshTile* InTile, const dtPoly* InPoly) const override
		{
			if (!Gate->bEntered.Exchange(true))
			{
				const double Deadline = FPlatformTime::Seconds() + 8.0;
				while (!Gate->bResume.Load() && FPlatformTime::Seconds() < Deadline) FPlatformProcess::Sleep(0.001f);
			}
			return FRecastQueryFilter::passVirtualFilter(InRef, InTile, InPoly);
		}

		virtual dtReal getVirtualCost(const dtReal* InStart, const dtReal* InEnd,
			const dtPolyRef InPrevious, const dtMeshTile* InPreviousTile, const dtPoly* InPreviousPoly,
			const dtPolyRef InCurrent, const dtMeshTile* InCurrentTile, const dtPoly* InCurrentPoly,
			const dtPolyRef InNext, const dtMeshTile* InNextTile, const dtPoly* InNextPoly) const override
		{
			Gate->CostCalls++;
			return 2.5 * FRecastQueryFilter::getVirtualCost(InStart, InEnd, InPrevious, InPreviousTile, InPreviousPoly,
				InCurrent, InCurrentTile, InCurrentPoly, InNext, InNextTile, InNextPoly);
		}

	private:
		TSharedRef<FVoxelNativeQueryTestGate, ESPMode::ThreadSafe> Gate;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNavigationNativeTileTest,
	"WHFramework.Voxel.Navigation.NativeTileCommitAndStaleInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNavigationNativeTileTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UWorld::InitializationValues Initialization;
	Initialization.CreatePhysicsScene(true).ShouldSimulatePhysics(true).CreateNavigation(false).CreateAISystem(true);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &Initialization);
	if (!TestNotNull(TEXT("原生tile夹具建立真实物理和导航世界"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* NavigationConfig = NewObject<UNavigationSystemModuleConfig>(World);
	NavigationConfig->NavigationSystemClass = FSoftClassPath(UVoxelNavigationSystem::StaticClass());
	FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::GameMode, NavigationConfig, false);
	auto* Owner = World->SpawnActor<AMainModule>();
	auto* Module = NewObject<UVoxelModule>(Owner);
	ON_SCOPE_EXIT
	{
		FString Error;
		Module->StopWorld(true, Error);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	if (!Owner->GetRootComponent())
	{
		auto* Root = NewObject<USceneComponent>(Owner);
		Owner->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	Module->bEnableProjectSceneSimulation = true;
	Module->SceneRegionClass = UVoxelNavigationTestSceneRegion::StaticClass();
	Module->Epoch = 43;
	Module->Manifest.BlockSizeCentimeters = 25;
	Module->NavigationClearanceCentimeters = 175.;
	Module->WorldState = EVoxelWorldState::Running;
	auto* Support = NewObject<UVoxelData>();
	Support->BlockName = TEXT("test:native_nav_floor");
	Support->DropCount = 0;
	FString Error;
	if (!TestTrue(TEXT("使用真实Registry"), Module->Registry.Build({Support}, false, Error))) return false;
	const uint16 Type = Module->Registry.GetSnapshot()->Find(Support->BlockName)->TypeId;
	const auto Shapes = MakeShared<FVoxelShapeRegistry, ESPMode::ThreadSafe>();
	Shapes->BuildDefaults();
	Module->Shapes = Shapes;
	Module->Generator = VoxelTest::MakeGenerator();
	Module->Runtime = MakeUnique<FVoxelWorldRuntime>(Module->Epoch, true,
		Module->Registry.GetSnapshot().ToSharedRef(), Module->Generator.ToSharedRef());
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
	if (!Module->TerrainFrameContext.Configure({}, Error)) return false;
	Module->Scheduler->BindFrameContext(Module->TerrainFrameContext);
	Module->CollisionPresenter = MakeUnique<FVoxelCollisionPresenter>(*Module, *Module->Scheduler, Module->Epoch);
	Module->NavigationPresenter = MakeUnique<FVoxelNavigationPresenter>(*Module, *Module->Scheduler);
	Module->ResidencyManager = MakeUnique<FVoxelResidencyManager>(*Module->Runtime, *Module->Scheduler);
	if (!TestTrue(TEXT("真实ResidencyManager按原控制身份初始化有界回调"), Module->ResidencyManager->Initialize(
		[Module](const FIntVector& Key) { Module->OnSectionEvicting(Key); }, [](const FIntVector&) { return false; },
		[Module](const FIntVector& Key, const FVoxelSectionStamp& Stamp, const EVoxelRuntimeSectionEvent Event)
		{
			Module->OnSimulationSectionEvent(Key, Stamp, Event);
		}, Error))) return false;
	const FIntVector Lower(0, 0, 0);
	const FIntVector Above(0, 0, 1);
	for (const FIntVector& Key : {Lower, Above})
	{
		auto* Section = Module->Runtime->FindOrAllocate(Key, 0);
		TArray<FVoxelBlockState> Blocks;
		Blocks.Init(FVoxelBlockState(), 4096);
		if (Key == Lower)
		{
			for (int32 Cell = 0; Cell < 256; ++Cell) Blocks[Cell + 16 * 16 * 15] = FVoxelBlockState(Type, 0);
			// 几何孔使真实路径跨多个polygon，成本委托不能仅由同poly快捷成功冒充。
			Blocks[8 + 16 * (7 + 16 * 15)] = FVoxelBlockState();
		}
		if (!Section || !Module->Runtime->PublishBase(Key, Section->Stamp, MoveTemp(Blocks), Error) ||
			!Module->Runtime->PublishFinal(Key, 0, {}, {}, Error)) return false;
	}
	const auto Interest = MakeShared<FVoxelInterestSet, ESPMode::ThreadSafe>();
	const auto Inputs = MakeShared<FVoxelTerrainPublicationInputs, ESPMode::ThreadSafe>();
	Inputs->RuntimeSections = {Lower, Above};
	Interest->PublicationInputs = Inputs;
	Interest->Exact.Add(Lower).bSimulation = true;
	Interest->Exact.FindChecked(Lower).bCollision = true;
	Interest->Exact.FindChecked(Lower).ServicePriority = {EVoxelTerrainServiceBand::P0, 0.0, 0.0, 0, FGuid::NewGuid()};
	Interest->Exact.Add(Above).bData = true;
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Center = FIntVector(8, 8, 16);
	Source.SimulationRadius = 16;
	Source.Capabilities = EVoxelStreamingCapability::Simulation;
	Source.Normalize();
	Interest->Sources.Add(Source.Id).Source = Source;
	Module->CurrentInterest = Interest;
	if (!TestTrue(TEXT("实际导航消费者先建立固定局部边界"), Module->NavigationPresenter->ActivateSection(Lower))) return false;
	auto* Navigation = FNavigationSystem::GetCurrent<UVoxelNavigationSystem>(World);
	if (!TestNotNull(TEXT("真实NavigationSystem已创建"), Navigation)) return false;
	FNavDataConfig Agent(24.f, 159.f);
	Agent.Name = TEXT("Default");
	Agent.SetNavDataClass(AVoxelNavigationData::StaticClass());
	Navigation->OverrideSupportedAgents({Agent});
	Navigation->OnWorldInitDone(FNavigationSystemRunMode::GameMode);
	Navigation->Tick(0.f);
	auto* Data = Cast<AVoxelNavigationData>(Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate));
	if (!TestNotNull(TEXT("夹具真实创建带tile提交证明的NavData"), Data)) return false;
	// 查询跨实际 tile 边界，保证成本委托不能由同 polygon 的快捷成功替代。
	Data->TileSizeUU = 200.f;
	Data->RebuildAll();
	Navigation->AddNavigationBuildLock(ENavigationBuildLock::Custom);
	Module->WakeSimulationSection(Lower);
	auto FindSurface = [&]() -> UVoxelNavigationSurfaceComponent*
	{
		TInlineComponentArray<UVoxelNavigationSurfaceComponent*> Components(Owner);
		return Components.IsEmpty() ? nullptr : Components[0];
	};
	uint64 Frame = 1;
	auto Pump = [&](TFunctionRef<bool()> Finished)
	{
		const double Deadline = FPlatformTime::Seconds() + 15.0;
		do
		{
			Module->TerrainFrameContext.Begin(Module->Epoch, ++Frame, FPlatformTime::Seconds(), {});
			Module->CollisionPresenter->Tick(Interest->Exact, Frame);
			Module->ReconcileSimulationDemand();
			Module->NavigationPresenter->Tick();
			Module->Scheduler->Tick([Module](FVoxelTaskResult&& Result)
			{
				Module->CollisionPresenter->OnTask(MoveTemp(Result));
			}, 8.0);
			if (const auto Retirement = Module->GetPhysicsRetirement()) Retirement->Advance(Module->TerrainFrameContext);
			Module->TerrainFrameContext.End(FPlatformTime::Seconds());
			World->Tick(LEVELTICK_All, 1.f / 60.f);
			// 同步 Automation 阻塞引擎主循环；显式排空真实 Solver 的命令和粒子输入。
			World->GetPhysicsScene()->Flush();
			Navigation->Tick(1.f / 60.f);
			FPlatformProcess::Sleep(0.001f);
		}
		while (!Finished() && FPlatformTime::Seconds() < Deadline);
		return Finished();
	};
	if (!TestTrue(TEXT("真实Collision提交后可导出完整地板"), Pump([&]()
	{
		return FindSurface() && !FindSurface()->GetSurface().Indices.IsEmpty();
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
	TestFalse(TEXT("仅Surface Apply不能提前宣告原生NavReady"), Module->IsNavigationReady(Lower));
	Navigation->RemoveNavigationBuildLock(ENavigationBuildLock::Custom);
	if (!TestTrue(TEXT("实际Recast构建和Detour提交形成NavReady"), Pump([&]()
	{
		return Module->IsNavigationReady(Lower) && Data->bQueryAdmissionOpen.Load();
	}))) return false;
	FNavLocation OriginalNode;
	if (!TestTrue(TEXT("Ready属于实际导航节点"), Navigation->ProjectPointToNavigation(FVector(200., 200., 400.),
		OriginalNode, FVector(50., 50., 100.), Data))) return false;
	const auto OriginalVersion = FindSurface()->GetSurface().InputVersion;
	TestTrue(TEXT("真实节点匹配冻结输入和tile salt"), Data->HasCurrentTileForNode(OriginalNode.NodeRef, OriginalVersion));
	const auto Gate = MakeShared<FVoxelNativeQueryTestGate, ESPMode::ThreadSafe>();
	const FVoxelBlockingNativeQueryFilter Blocking(*static_cast<const FRecastQueryFilter*>(Data->GetDefaultQueryFilter()->GetImplementation()), Gate);
	const auto CallerFilter = MakeShared<FNavigationQueryFilter, ESPMode::ThreadSafe>();
	CallerFilter->SetFilterImplementation(&Blocking);
	FPathFindingQuery Query(Module, *Data, FVector(100., 187.5, 400.), FVector(300., 187.5, 400.), CallerFilter);
	auto QueryPath = Data->CreatePathInstance<FNavMeshPath>(Query);
	QueryPath->SetSourceActor(*Owner);
	QueryPath->SetGoalActorObservation(*Owner, 50.);
	Query.PathInstanceToFill = QueryPath;
	auto RunningQuery = UE::Tasks::Launch(UE_SOURCE_LOCATION, [Data, Query]()
	{
		return Data->ANavigationData::FindPath(Query.NavAgentProperties, Query);
	});
	const double QueryEntryDeadline = FPlatformTime::Seconds() + 2.0;
	while (!Gate->bEntered.Load() && FPlatformTime::Seconds() < QueryEntryDeadline) FPlatformProcess::Sleep(0.001f);
	if (!TestTrue(TEXT("实际后台FindPath进入调用者虚拟过滤器"), Gate->bEntered.Load()))
	{
		Gate->bResume.Store(true);
		RunningQuery.Wait();
		AddInfo(FString::Printf(TEXT("Native query entry: result=%d admission=%d readers=%d rebuild=%d bounds=%d worldClosing=%d committedTiles=%d"),
			int32(RunningQuery.GetResult().Result), Data->bQueryAdmissionOpen.Load(), Data->NativeQueryReaders.Load(),
			Data->bNativeRebuildPending, Data->bNativeBoundsPending, Data->bWorldCloseRequested, Data->CommittedTiles.Num()));
		return false;
	}
	TestEqual(TEXT("真实查询持有原生读取屏障"), Data->NativeQueryReaders.Load(), 1);
	TestFalse(TEXT("同一真实路径对象不能被并行查询再次Reset/写入"), Data->ANavigationData::FindPath(Query.NavAgentProperties, Query).IsSuccessful());
	TestFalse(TEXT("读取中的真实查询阻止世界epoch/输入提前关闭"), Module->StopWorld(true, Error));
	TestEqual(TEXT("原生查询停止等待不改变世界epoch"), Module->Epoch, uint64(43));
	const FGuid NativeBeforeDeferredRebuild = Data->EvidenceOwnerId;
	Data->RebuildAll();
	TestTrue(TEXT("GT重建冲突保留原生意图并直接yield"), Data->bNativeRebuildPending &&
		Data->EvidenceOwnerId == NativeBeforeDeferredRebuild);
	TestFalse(TEXT("待写入期间关闭新查询接纳"), Data->ANavigationData::FindPath(Query.NavAgentProperties, Query).IsSuccessful());
	Gate->bResume.Store(true);
	RunningQuery.Wait();
	auto QueryResult = RunningQuery.GetResult();
	TestTrue(TEXT("读取者归还前原生池没有换代，原查询按其实际输入完成"), QueryResult.IsSuccessful());
	TestEqual(TEXT("实际查询执行退出后原生读取计数归零"), Data->NativeQueryReaders.Load(), 0);
	TestTrue(TEXT("输入约束适配仍委托调用者虚拟成本"), Gate->CostCalls.Load() > 0);
	if (QueryResult.Path)
	{
		TestTrue(TEXT("实际查询继续填充调用者原路径对象"), QueryResult.Path == QueryPath);
		TestTrue(TEXT("原路径目标观察与Source设置跨真实Reset保留"), QueryResult.Path->GetGoalActor() == Owner &&
			QueryResult.Path->GetPathFindingStartLocation() == Owner->GetActorLocation());
		TestTrue(TEXT("原查询过滤器恢复到实际返回路径，范围适配不逸出"), QueryResult.Path->GetFilter() == CallerFilter);
		TestTrue(TEXT("原查询数据保留调用者原终点"), QueryResult.Path->GetQueryData().EndLocation == Query.EndLocation);
		TestTrue(TEXT("调用者端点调整仍作用于实际Detour路径"), FMath::IsNearlyEqual(QueryResult.Path->GetEndLocation().X, 275., 1.));
		QueryResult.Path->EnableRecalculationOnInvalidation(false);
		QueryResult.Path->SetIgnoreInvalidation(true);
		TestTrue(TEXT("实际返回路径在查询退出后仍持有Section消费者引用"),
			Module->NavigationPresenter->HasReaders());
	}
	Data->RebuildAll();
	if (QueryResult.Path && !Data->bNativeRebuildPending)
	{
		TestFalse(TEXT("Native池真实重建前立即撤销已完成路径，即使忽略普通失效"), QueryResult.Path->IsValid());
		TestFalse(TEXT("撤销改变实际引擎路径状态，不只改变外部版本谓词"), QueryResult.Path->IsUpToDate());
	}
	if (!TestTrue(TEXT("查询归还后既有原生Tick恢复待重建并重建当前输入"), Pump([&]()
	{
		return Data->EvidenceOwnerId != NativeBeforeDeferredRebuild && Module->IsNavigationReady(Lower);
	}))) return false;
	if (QueryResult.Path)
	{
		TestFalse(TEXT("实际重建完成后原路径保持已撤销，不能复用新tile代"), QueryResult.Path->IsValid());
	}
	Query.PathInstanceToFill.Reset();
	QueryPath.Reset();
	QueryResult.Path.Reset();
	RunningQuery = {};
	FPathFindingQuery ActiveQuery(Module, *Data, FVector(100., 100., 400.), FVector(300., 300., 400.));
	auto ActiveResult = Data->ANavigationData::FindPath(ActiveQuery.NavAgentProperties, ActiveQuery);
	// Section 中心的当前 tile 就绪不代表两个端点的异步 tile 都已提交；等待原生路径自身的完成证明。
	if (!TestTrue(TEXT("建立实际活动路径用于寿命验证"), Pump([&]()
	{
		ActiveResult = Data->ANavigationData::FindPath(ActiveQuery.NavAgentProperties, ActiveQuery);
		return ActiveResult.IsSuccessful() && ActiveResult.Path && ActiveResult.Path->IsValid();
	})))
	{
		FNavLocation StartNode, EndNode;
		const bool bStartProjected = Navigation->ProjectPointToNavigation(ActiveQuery.StartLocation, StartNode, Data->GetDefaultQueryExtent(), Data);
		const bool bEndProjected = Navigation->ProjectPointToNavigation(ActiveQuery.EndLocation, EndNode, Data->GetDefaultQueryExtent(), Data);
		AddInfo(FString::Printf(TEXT("Native active path: result=%d ready=%d valid=%d points=%d admission=%d deferred=%d worldClose=%d tiles=%d pathInputs=%d startProjected=%d endProjected=%d startCurrent=%d endCurrent=%d navReady=%d"),
			int32(ActiveResult.Result), ActiveResult.Path && ActiveResult.Path->IsReady(), ActiveResult.Path && ActiveResult.Path->IsValid(),
			ActiveResult.Path ? ActiveResult.Path->GetPathPoints().Num() : 0, Data->bQueryAdmissionOpen.Load(), Data->HasDeferredNativeWrite(),
			Data->bWorldCloseRequested, Data->CommittedTiles.Num(), Data->PathInputs.Num(), bStartProjected, bEndProjected,
			bStartProjected && Data->HasCurrentTileForNode(StartNode.NodeRef, FindSurface()->GetSurface().InputVersion),
			bEndProjected && Data->HasCurrentTileForNode(EndNode.NodeRef, FindSurface()->GetSurface().InputVersion), Module->IsNavigationReady(Lower)));
		return false;
	}
	ActiveResult.Path->EnableRecalculationOnInvalidation(false);
	Module->NavigationPresenter->RetireSection(Lower);
	TestTrue(TEXT("Nav Source撤销时实际路径仍保留lower/upper与物理需求"),
		Module->NavigationPresenter->IsResident(Lower) && Module->NavigationPresenter->HasReaders());
	TestTrue(TEXT("路径保留的原输入仍可用于实际行走"), ActiveResult.Path->IsValid());
	FPathFindingQuery ContinueQuery(ActiveQuery);
	ContinueQuery.PathInstanceToFill = ActiveResult.Path;
	auto ContinuedResult = Data->ANavigationData::FindPath(ContinueQuery.NavAgentProperties, ContinueQuery);
	TestTrue(TEXT("Source普通接纳已关时，原真实路径可凭旧读取权限在同对象repath"),
		ContinuedResult.IsSuccessful() && ContinuedResult.Path == ActiveResult.Path);
	ContinueQuery.PathInstanceToFill.Reset();
	ContinuedResult.Path.Reset();
	if (!TestTrue(TEXT("原Nav消费者恢复沿用待退役输入"), Module->NavigationPresenter->ActivateSection(Lower))) return false;
	ActiveResult.Path.Reset();
	if (!TestTrue(TEXT("真实路径最后引用退出后，既有GT队列归还所有Section读取"), Pump([&]()
	{
		return !Module->NavigationPresenter->HasReaders();
	}))) return false;
	FPathFindingQuery AsyncQuery(Module, *Data, FVector(100., 100., 400.), FVector(300., 300., 400.));
	const auto AsyncPath = Data->CreatePathInstance<FNavMeshPath>(AsyncQuery);
	AsyncPath->EnableRecalculationOnInvalidation(false);
	AsyncQuery.PathInstanceToFill = AsyncPath;
	const auto AsyncResult = MakeShared<FVoxelNativeAsyncQueryResult, ESPMode::ThreadSafe>();
	const uint32 AsyncId = Navigation->FindPathAsync(AsyncQuery.NavAgentProperties, AsyncQuery,
		FNavPathQueryDelegate::CreateLambda([AsyncResult](const uint32 InId, const ENavigationQueryResult::Type InResult,
			FNavPathSharedPtr InPath)
		{
			AsyncResult->bCalled = true;
			AsyncResult->QueryId = InId;
			AsyncResult->Result = InResult;
			AsyncResult->bPathValidAtDispatch = InPath && InPath->IsValid();
			AsyncResult->Path = MoveTemp(InPath);
		}));
	if (!TestTrue(TEXT("真实引擎异步路径请求得到有效Id"), AsyncId != INVALID_NAVQUERYID)) return false;
	Navigation->Tick(1.f / 60.f);
	bool bAsyncFinished = false;
	const double AsyncDeadline = FPlatformTime::Seconds() + 3.0;
	while (!bAsyncFinished && FPlatformTime::Seconds() < AsyncDeadline)
	{
		{
			FScopeLock Lock(&Data->PathInputsLock);
			bAsyncFinished = Data->PathInputs.Contains(AsyncPath.Get()) && Data->NativeQueryReaders.Load() == 0 &&
				!Navigation->IsTerrainQueryBatchRunning();
		}
		if (!bAsyncFinished) FPlatformProcess::Sleep(0.001f);
	}
	if (!TestTrue(TEXT("真实引擎Worker已发布原路径，GT尚未派发回调"), bAsyncFinished && !AsyncResult->bCalled)) return false;
	Gate->bEntered.Store(false);
	Gate->bResume.Store(false);
	auto StaleRunningQuery = UE::Tasks::Launch(UE_SOURCE_LOCATION, [Data, Query]()
	{
		return Data->ANavigationData::FindPath(Query.NavAgentProperties, Query);
	});
	const double StaleEntryDeadline = FPlatformTime::Seconds() + 2.0;
	while (!Gate->bEntered.Load() && FPlatformTime::Seconds() < StaleEntryDeadline) FPlatformProcess::Sleep(0.001f);
	if (!TestTrue(TEXT("另一真实查询在旧净空输入上处于执行中"), Gate->bEntered.Load()))
	{
		Gate->bResume.Store(true);
		StaleRunningQuery.Wait();
		return false;
	}
	ON_SCOPE_EXIT
	{
		Gate->bResume.Store(true);
		StaleRunningQuery.Wait();
	};
	Navigation->AddNavigationBuildLock(ENavigationBuildLock::Custom);
	FVoxelCellEdit Ceiling;
	Ceiling.Position = FIntVector(8, 8, 19);
	Ceiling.Value = FVoxelBlockState(Type, 0);
	FVoxelPreparedEdit Prepared;
	FVoxelEditBatch Batch;
	if (!Module->Runtime->PrepareEdit({Ceiling}, {}, Prepared, Error) ||
		!Module->Runtime->CommitPreparedEdit(MoveTemp(Prepared), Batch, Error)) return false;
	Module->PublishProjectEdit(Batch);
	TestFalse(TEXT("上层净空变更在GT回调前立即撤销真实异步结果路径"), AsyncPath->IsValid());
	TestFalse(TEXT("上层输入撤销不等待Recast重新生成才改变引擎路径状态"), AsyncPath->IsUpToDate());
	TestTrue(TEXT("失效结果仍由使用者持有时保留Nav/Data/Physics语义需求"), Module->NavigationPresenter->HasReaders());
	Gate->bResume.Store(true);
	StaleRunningQuery.Wait();
	const auto StaleRunningResult = StaleRunningQuery.GetResult();
	TestFalse(TEXT("输入在实际搜索中换代后，查询不能成功发布旧输入"), StaleRunningResult.IsSuccessful());
	if (StaleRunningResult.Path)
	{
		TestFalse(TEXT("搜索中被撤销的真实路径不保留引擎Ready状态"), StaleRunningResult.Path->IsReady());
	}
	Navigation->Tick(1.f / 60.f);
	TestTrue(TEXT("原引擎异步请求仍正常派发到原Id和原路径对象"),
		AsyncResult->bCalled && AsyncResult->QueryId == AsyncId && AsyncResult->Path == AsyncPath);
	TestFalse(TEXT("Worker完成到GT派发窗口中的旧路径不可供回调消费者使用"), AsyncResult->bPathValidAtDispatch);
	TestEqual(TEXT("旧输入不能通过缓存Success枚举进入原GT回调"), AsyncResult->Result, ENavigationQueryResult::Fail);
	FNavLocation OldProjection;
	TestTrue(TEXT("构建锁保留真实旧tile，不能靠投影结果冒充新版完成"),
		Navigation->ProjectPointToNavigation(FVector(200., 200., 400.), OldProjection, FVector(50., 50., 100.), Data));
	TestFalse(TEXT("上层净空换代立即使旧tile证明失效"), Data->HasCurrentTileForNode(OldProjection.NodeRef, OriginalVersion));
	TestFalse(TEXT("旧tile仍能投影时NavReady保持等待"), Module->IsNavigationReady(Lower));
	FPathFindingQuery StaleQuery(Module, *Data, FVector(100., 100., 400.), FVector(300., 300., 400.));
	TestFalse(TEXT("实际FindPath不能使用仍能投影的旧输入tile"), Data->ANavigationData::FindPath(StaleQuery.NavAgentProperties, StaleQuery).IsSuccessful());
	TestFalse(TEXT("实际TestPath同样使用完整当前输入约束"), Data->ANavigationData::TestPath(StaleQuery.NavAgentProperties, StaleQuery, nullptr));
	Navigation->RemoveNavigationBuildLock(ENavigationBuildLock::Custom);
	if (!TestTrue(TEXT("局部重建恢复完整当前版本"), Pump([&]() { return Module->IsNavigationReady(Lower); }))) return false;
	TestFalse(TEXT("新提交不能把旧节点重新标记为当前"), Data->HasCurrentTileForNode(OriginalNode.NodeRef, OriginalVersion));
	{
		const auto BatchGate = MakeShared<FVoxelNativeQueryTestGate, ESPMode::ThreadSafe>();
		ON_SCOPE_EXIT { BatchGate->bResume.Store(true); };
		const auto BatchFilter = MakeShared<FNavigationQueryFilter, ESPMode::ThreadSafe>();
		const FVoxelBlockingNativeQueryFilter BatchImplementation(
			*static_cast<const FRecastQueryFilter*>(Data->GetDefaultQueryFilter()->GetImplementation()), BatchGate);
		BatchFilter->SetFilterImplementation(&BatchImplementation);
		FPathFindingQuery BatchQuery(Module, *Data, FVector(100., 100., 400.), FVector(300., 300., 400.), BatchFilter);
		const auto BatchResult = MakeShared<FVoxelNativeAsyncQueryResult, ESPMode::ThreadSafe>();
		const auto QueuedResult = MakeShared<FVoxelNativeAsyncQueryResult, ESPMode::ThreadSafe>();
		auto Record = [](TSharedRef<FVoxelNativeAsyncQueryResult, ESPMode::ThreadSafe> InResult)
		{
			return FNavPathQueryDelegate::CreateLambda([InResult](const uint32 InId,
				const ENavigationQueryResult::Type InOutcome, FNavPathSharedPtr InPath)
			{
				InResult->bCalled = true;
				InResult->QueryId = InId;
				InResult->Result = InOutcome;
				InResult->Path = MoveTemp(InPath);
			});
		};
		const uint32 BatchId = Navigation->FindPathAsync(BatchQuery.NavAgentProperties, BatchQuery, Record(BatchResult));
		if (!TestTrue(TEXT("真实异步批次请求有原始Id"), BatchId != INVALID_NAVQUERYID)) return false;
		Navigation->Tick(1.f / 60.f);
		const double EnterDeadline = FPlatformTime::Seconds() + 3.0;
		while (!BatchGate->bEntered.Load() && FPlatformTime::Seconds() < EnterDeadline) FPlatformProcess::Sleep(0.001f);
		if (!TestTrue(TEXT("真实NavigationSystem任务已在原生过滤器内部运行"), BatchGate->bEntered.Load() &&
			Navigation->IsTerrainQueryBatchRunning())) return false;
		const double NoWaitStart = FPlatformTime::Seconds();
		Navigation->Tick(1.f / 60.f);
		FWorldDelegates::OnWorldPostActorTick.Broadcast(World, LEVELTICK_All, 1.f / 60.f);
		TestTrue(TEXT("真实系统Tick和帧后回调不join被阻塞的引擎异步批次"),
			FPlatformTime::Seconds() - NoWaitStart < 0.25 && !BatchGate->bResume.Load());
		FPathFindingQuery QueuedQuery(Module, *Data, FVector(100., 100., 400.), FVector(300., 300., 400.));
		const uint32 QueuedId = Navigation->FindPathAsync(QueuedQuery.NavAgentProperties, QueuedQuery, Record(QueuedResult));
		TestTrue(TEXT("批次运行时新增请求仍取得自己的原Id"), QueuedId != INVALID_NAVQUERYID && QueuedId != BatchId);
		TestFalse(TEXT("整个真实异步批次未退出时StopWorld拒绝换代"), Module->StopWorld(false, Error));
		TestEqual(TEXT("实际批次等待期间保持原世界代"), Module->GetWorldEpoch(), uint64(43));
		BatchGate->bResume.Store(true);
		const double FinishDeadline = FPlatformTime::Seconds() + 3.0;
		while (Navigation->IsTerrainQueryBatchRunning() && FPlatformTime::Seconds() < FinishDeadline) FPlatformProcess::Sleep(0.001f);
		if (!TestFalse(TEXT("真实引擎任务已完成，尚未GT派发"), Navigation->IsTerrainQueryBatchRunning())) return false;
		TestTrue(TEXT("退出准备撤回原世界排队请求和完成但未派发的结果"), Navigation->TryCloseTerrainQueries(*Data));
		Navigation->Tick(1.f / 60.f);
		TestFalse(TEXT("被退出准备撤回的完成请求不在以后派发"), BatchResult->bCalled);
		TestFalse(TEXT("被退出准备撤回的排队请求不进入下一个批次"), QueuedResult->bCalled);
		const auto CurrentResult = MakeShared<FVoxelNativeAsyncQueryResult, ESPMode::ThreadSafe>();
		const uint32 CurrentId = Navigation->FindPathAsync(QueuedQuery.NavAgentProperties, QueuedQuery, Record(CurrentResult));
		if (!TestTrue(TEXT("同一仍运行世界可接纳后续正常请求"), CurrentId != INVALID_NAVQUERYID && Pump([&]()
		{
			return CurrentResult->bCalled;
		}))) return false;
		TestTrue(TEXT("当前请求仍按其原QueryId派发真实Success与有效路径"),
			CurrentResult->QueryId == CurrentId && CurrentResult->Result == ENavigationQueryResult::Success &&
			CurrentResult->Path && CurrentResult->Path->IsValid());
	}
	{
		auto MakePathActor = [&](const FVector& InLocation)
		{
			auto* Actor = World->SpawnActor<AActor>();
			auto* Root = NewObject<USceneComponent>(Actor);
			Actor->SetRootComponent(Root);
			Root->RegisterComponent();
			Actor->SetActorLocation(InLocation);
			return Actor;
		};
		auto* PathSource = MakePathActor(FVector(100., 100., 400.));
		auto* PathGoal = MakePathActor(FVector(300., 300., 400.));
		FPathFindingQuery RepathQuery(Module, *Data, PathSource->GetActorLocation(), PathGoal->GetActorLocation());
		auto RepathResult = Data->ANavigationData::FindPath(RepathQuery.NavAgentProperties, RepathQuery);
		if (!TestTrue(TEXT("自动重规划从真实当前Detour路径开始"), RepathResult.IsSuccessful() &&
			RepathResult.Path && RepathResult.Path->IsValid())) return false;
		const auto Path = RepathResult.Path;
		Path->SetSourceActor(*PathSource);
		Path->SetGoalActorObservation(*PathGoal, 1.f);
		const auto Events = MakeShared<FVoxelNativeRepathEvents, ESPMode::ThreadSafe>();
		Path->AddObserver(FNavigationPath::FPathObserverDelegate::FDelegate::CreateLambda(
			[Events](FNavigationPath*, const ENavPathEvent::Type InEvent)
			{
				if (InEvent == ENavPathEvent::Invalidated) ++Events->Invalidated;
				if (InEvent == ENavPathEvent::UpdatedDueToNavigationChanged) ++Events->NavigationUpdated;
				if (InEvent == ENavPathEvent::UpdatedDueToGoalMoved) ++Events->GoalUpdated;
				if (InEvent == ENavPathEvent::RePathFailed) ++Events->Failed;
			}));
		auto ChangeClearance = [&](const int32 InX)
		{
			FVoxelCellEdit Edit;
			Edit.Position = FIntVector(InX, 8, 19);
			Edit.Value = FVoxelBlockState(Type, 0);
			FVoxelPreparedEdit LocalPrepared;
			FVoxelEditBatch LocalBatch;
			if (!Module->Runtime->PrepareEdit({Edit}, {}, LocalPrepared, Error) ||
				!Module->Runtime->CommitPreparedEdit(MoveTemp(LocalPrepared), LocalBatch, Error)) return false;
			Module->PublishProjectEdit(LocalBatch);
			return true;
		};
		auto HasWaitingCompletion = [&]()
		{
			const auto* Id = Data->PendingRepathPaths.Find(Path.Get());
			const auto* Control = Id ? Data->PendingRepaths.Find(*Id) : nullptr;
			return Control && (*Control)->bCompletionPending && (*Control)->bWaiting;
		};
		Navigation->AddNavigationBuildLock(ENavigationBuildLock::Custom);
		ON_SCOPE_EXIT { Navigation->RemoveNavigationBuildLock(ENavigationBuildLock::Custom); };
		if (!ChangeClearance(9)) return false;
		TestTrue(TEXT("真实输入撤销触发原路径自动重规划和失效观察者"),
			Events->Invalidated > 0 && Path->IsWaitingForRepath() && !Path->IsValid());
		if (!TestTrue(TEXT("真实异步自动重规划把缺失输入分类为等待"), Pump(HasWaitingCompletion))) return false;
		for (int32 WaitFrame = 0; WaitFrame < 6; ++WaitFrame) Pump([]() { return true; });
		TestTrue(TEXT("构建锁期间原路径保持等待与原Nav语义需求"),
			Path->IsWaitingForRepath() && Module->NavigationPresenter->HasReaders());
		TestEqual(TEXT("输入等待不能广播最终重规划失败"), Events->Failed, 0);
		TestEqual(TEXT("旧tile不能广播成功更新"), Events->NavigationUpdated, 0);
		Navigation->RemoveNavigationBuildLock(ENavigationBuildLock::Custom);
		if (!TestTrue(TEXT("新版真实tile到达后同一路径自动恢复并通知原观察者"), Pump([&]()
		{
			return Events->NavigationUpdated > 0 && Path->IsValid() && !Path->IsWaitingForRepath();
		}))) return false;
		TestTrue(TEXT("自动重规划保留真实Source、Goal、调用者filter与原路径对象"),
			RepathResult.Path == Path && Path->GetSourceActor() == PathSource && Path->GetGoalActor() == PathGoal &&
			Path->GetFilter() == RepathQuery.QueryFilter);
		auto AdvanceMaintenance = [&]()
		{
			Module->TerrainFrameContext.Begin(Module->Epoch, ++Frame, FPlatformTime::Seconds(), {});
			Data->AdvancePathMaintenance(Module->TerrainFrameContext);
			Module->TerrainFrameContext.End(FPlatformTime::Seconds());
		};
		auto AdmitPending = [&]()
		{
			const double Deadline = FPlatformTime::Seconds() + 3.0;
			do
			{
				AdvanceMaintenance();
				if (Data->PendingRepathPaths.Contains(Path.Get())) return true;
				FPlatformProcess::Sleep(0.001f);
			}
			while (FPlatformTime::Seconds() < Deadline);
			return false;
		};
		const int32 BeforeLateRevocation = Events->NavigationUpdated;
		Path->Invalidate();
		if (!TestTrue(TEXT("原路径失效请求在共享预算内实际接纳"), AdmitPending())) return false;
		Navigation->Tick(1.f / 60.f);
		const double RepathDeadline = FPlatformTime::Seconds() + 3.0;
		while (Navigation->IsTerrainQueryBatchRunning() && FPlatformTime::Seconds() < RepathDeadline) FPlatformProcess::Sleep(0.001f);
		if (!TestFalse(TEXT("原自动重规划实际引擎任务退出"), Navigation->IsTerrainQueryBatchRunning())) return false;
		Navigation->Tick(1.f / 60.f);
		const auto* LateId = Data->PendingRepathPaths.Find(Path.Get());
		const auto* LateControl = LateId ? Data->PendingRepaths.Find(*LateId) : nullptr;
		if (!TestTrue(TEXT("实际自动重规划delegate已完成，尚未帧预算结算"),
			LateControl && (*LateControl)->bCompletionPending)) return false;
		Navigation->AddNavigationBuildLock(ENavigationBuildLock::Custom);
		if (!ChangeClearance(10)) return false;
		AdvanceMaintenance();
		TestEqual(TEXT("delegate之后撤销输入也不能恢复旧Success状态"), Events->NavigationUpdated, BeforeLateRevocation);
		TestTrue(TEXT("帧结算再次撤销旧成功结果并保留等待"), !Path->IsValid() && Path->IsWaitingForRepath());
		Navigation->RemoveNavigationBuildLock(ENavigationBuildLock::Custom);
		if (!TestTrue(TEXT("派发后的撤销仍沿同一路径恢复"), Pump([&]()
		{
			return Events->NavigationUpdated > BeforeLateRevocation && Path->IsValid() && !Path->IsWaitingForRepath();
		}))) return false;
		PathGoal->SetActorLocation(FVector(300., 200., 400.));
		if (!TestTrue(TEXT("真实Goal观察在分帧处理后触发同对象异步重规划"), Pump([&]()
		{
			return Events->GoalUpdated > 0 && Path->IsValid() && !Path->IsWaitingForRepath();
		}))) return false;
		TestTrue(TEXT("Goal移动采用当前真实Actor终点"), Path->GetQueryData().EndLocation.Equals(PathGoal->GetActorLocation(), 1.));
		const int32 BeforeLateGoal = Events->GoalUpdated;
		PathGoal->SetActorLocation(FVector(200., 200., 400.));
		Data->RequestRePath(Path, ENavPathUpdateType::GoalMoved);
		if (!TestTrue(TEXT("Goal原始请求接纳进真实异步队列"), AdmitPending())) return false;
		Navigation->Tick(1.f / 60.f);
		const double GoalDeadline = FPlatformTime::Seconds() + 3.0;
		while (Navigation->IsTerrainQueryBatchRunning() && FPlatformTime::Seconds() < GoalDeadline) FPlatformProcess::Sleep(0.001f);
		if (!TestFalse(TEXT("旧目标查询的实际引擎任务已经退出"), Navigation->IsTerrainQueryBatchRunning())) return false;
		PathGoal->SetActorLocation(FVector(200., 300., 400.));
		Navigation->Tick(1.f / 60.f);
		AdvanceMaintenance();
		TestEqual(TEXT("寻路后Goal再次移动不能广播旧目标的成功更新"), Events->GoalUpdated, BeforeLateGoal);
		TestTrue(TEXT("旧终点尝试保留等待，不能冒充当前Goal路径"), Path->IsWaitingForRepath());
		if (!TestTrue(TEXT("旧目标尝试退出后在同路径采用最新Goal"), Pump([&]()
		{
			return Events->GoalUpdated > BeforeLateGoal && Path->IsValid() && !Path->IsWaitingForRepath();
		}))) return false;
		TestTrue(TEXT("最终路径query data采用第二次移动的真实终点"), Path->GetQueryData().EndLocation.Equals(PathGoal->GetActorLocation(), 1.));
		const int32 BeforeSuperseded = Events->NavigationUpdated;
		Data->RequestRePath(Path, ENavPathUpdateType::NavigationChanged);
		if (!TestTrue(TEXT("旧重规划请求先真实排队"), AdmitPending())) return false;
		PathGoal->SetActorLocation(FVector(300., 100., 400.));
		FPathFindingQuery ReplacementQuery(Module, *Data, PathSource->GetActorLocation(), PathGoal->GetActorLocation(), Path->GetFilter());
		ReplacementQuery.SetPathInstanceToUpdate(Path);
		auto ReplacementResult = Data->ANavigationData::FindPath(ReplacementQuery.NavAgentProperties, ReplacementQuery);
		if (!TestTrue(TEXT("独立真实新尝试先在原对象发布当前路径"), ReplacementResult.IsSuccessful() &&
			ReplacementResult.Path == Path && Path->IsValid())) return false;
		Path->DoneUpdating(ENavPathUpdateType::Custom);
		Path->UpdateLastRepathGoalLocation();
		const auto ReplacementInputs = Data->FindPathInputs(Path);
		Navigation->Tick(1.f / 60.f);
		const double SupersededDeadline = FPlatformTime::Seconds() + 3.0;
		while (Navigation->IsTerrainQueryBatchRunning() && FPlatformTime::Seconds() < SupersededDeadline) FPlatformProcess::Sleep(0.001f);
		if (!TestFalse(TEXT("旧排队尝试实际引擎任务已经退出"), Navigation->IsTerrainQueryBatchRunning())) return false;
		Navigation->Tick(1.f / 60.f);
		AdvanceMaintenance();
		TestTrue(TEXT("旧任务在Reset前拒绝已被替换的实际输入尝试"), Path->IsValid() &&
			Data->FindPathInputs(Path) == ReplacementInputs && Path->GetQueryData().EndLocation.Equals(PathGoal->GetActorLocation(), 1.));
		TestFalse(TEXT("旧完成不能把新路径重新标记为等待"), Path->IsWaitingForRepath());
		TestEqual(TEXT("旧完成不能广播新尝试的NavigationChanged成功"), Events->NavigationUpdated, BeforeSuperseded);
		TestEqual(TEXT("真实输入等待与Goal移动都没有最终失败事件"), Events->Failed, 0);
		Path->DisableGoalActorObservation();
		Path->EnableRecalculationOnInvalidation(false);
	}
	for (int32 Rebuild = 0; Rebuild < 3; ++Rebuild)
	{
		const FGuid PreviousNativeOwner = Data->EvidenceOwnerId;
		Data->RebuildAll();
		TestTrue(TEXT("同世界Detour重建已实际换代或保留未完成的原生任务等待"),
			Data->EvidenceOwnerId != PreviousNativeOwner || Data->bNativeRebuildPending);
		if (!Data->bNativeRebuildPending) TestEqual(TEXT("Native重建在新提交前清空旧slot证明"), Data->CommittedTiles.Num(), 0);
		if (!TestTrue(TEXT("实际Native重建重新建立当前输入证明"), Pump([&]()
		{
			return Data->EvidenceOwnerId != PreviousNativeOwner && Module->IsNavigationReady(Lower);
		}))) return false;
		TestTrue(TEXT("salt换代和全重建不累计历史Ref目录"), Data->CommittedTiles.Num() <= Data->EvidenceCapacity);
	}
	bool bStopped = false;
	if (!TestTrue(TEXT("停止世界撤销路径需求并等待真实输入归还后撤销Nav owner"), Pump([&]()
	{
		if (!bStopped) bStopped = Module->StopWorld(true, Error);
		return bStopped;
	}))) return false;
	TestFalse(TEXT("旧世界tile只可清理，不能成为新世界的Ready证据"), Data->HasCurrentTileForNode(OriginalNode.NodeRef, OriginalVersion));
	return true;
}

#endif
