#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Rendering/VoxelHeightfieldPageMesh.h"
#include "Voxel/Rendering/VoxelProxyPageMesh.h"
#include "Voxel/Rendering/VoxelWaterView.h"
#include "Voxel/Rendering/VoxelFineHandoff.h"
#include "VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownInitialInterestIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.InitialInterestIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownInitialInterestIntegrationTest::RunTest(const FString& InParameters)
{
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -256;
	Manifest.Settings.MaxZ = 256;
	FVoxelViewSettings Settings;
	Settings.DefaultSourceView.FineLevel0RadiusCells = 1;
	Settings.DefaultSourceView.FineLevel0VerticalHalfExtentCells = 1;
	Settings.DefaultSourceView.CollisionGuardRadiusCells = 2;
	Settings.DefaultSourceView.FineRadiusCells = 64;
	Settings.DefaultSourceView.FineVerticalHalfExtentCells = 64;
	Settings.Fine.MaximumPageLevel = 1;
	Settings.Fine.FirstMergeDistanceCells = 32;
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Center = FIntVector(8);
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual;
	Source.bInheritWorldView = false;
	Source.ExactRadius = -1;
	Source.View = Settings.DefaultSourceView;
	Source.CollisionRadius = 0;
	Source.SimulationRadius = 0;
	for (const int32 Initial : {16, 32})
	{
		Source.View.InitialChunkSideCells = Initial;
		const FVoxelInterestSet Interest = FVoxelInterestManager().Compute(MakeArrayView(&Source, 1), Manifest, Settings);
		TestTrue(TEXT("Real interest planner succeeds"), Interest.PlanningError.IsEmpty());
		if (!Interest.PlanningError.IsEmpty()) AddError(Interest.PlanningError);
		if (!Interest.FinePagePlan)
		{
			AddError(TEXT("Real interest is missing the shared Fine page plan"));
			return false;
		}
		TestTrue(TEXT("实际Fine驻留窗口包含Core之外的Shard"), Interest.FinePagePlan->Pages.Num() > 1);
		const auto* Accepted = Interest.Sources.Find(Source.Id);
		if (!TestNotNull(TEXT("实际Source保留F0需求"), Accepted)) return false;
		TestEqual(TEXT("F0包含交互核心的物理Halo"), Accepted->F0CoreSections.Num(), 27);
		for (const auto& Pair : Interest.FinePagePlan->SectionOwners)
		{
			const auto* Demand = Interest.Exact.Find(Pair.Key);
			if (Accepted->F0CoreSections.Contains(Pair.Key))
			{
				TestTrue(TEXT("F0保留真实Fine数据和渲染需求"), Demand && Demand->bData && Demand->bFineData && Demand->bFineRender);
			}
			else
			{
				TestTrue(TEXT("F1Page不能扩大Runtime渲染闭包"), !Demand || !Demand->bFineRender);
			}
			TestTrue(TEXT("FinePage不会隐式扩张碰撞或模拟"), !Demand || (!Demand->bCollision && !Demand->bSimulation));
		}
	}
	FVoxelStreamingSource Previous = Source;
	Previous.View.InitialChunkSideCells = 16;
	TestTrue(TEXT("Size changes invalidate the effective source"), FVoxelInterestRuntime::NeedsUpdate(Previous, Source, Settings));
	Settings.Fine.ShardSideCells = 24;
	const FVoxelInterestSet Invalid = FVoxelInterestManager().Compute(MakeArrayView(&Source, 1), Manifest, Settings);
	TestFalse(TEXT("实际规划器拒绝非法FineShard尺寸"), Invalid.PlanningError.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownEmptyPageIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.EmptyPageIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownEmptyPageIntegrationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelFineHandoff Handoff(1);
	FVoxelFineOwnershipChange Change;
	Change.Section = FIntVector(-1, 0, 3);
	Change.After.Owner = {EVoxelFineOwnerKind::F1Plus, FGuid::NewGuid(), 1};
	Change.After.Content = {1, 17, 23, 31, 41, 1, 1, 1, FGuid::NewGuid(), 1};
	Change.After.bGeometryReady = Change.After.bCanStage = Change.After.bKnownEmpty = true;
	Change.After.GeometryLease = MakeShared<FVoxelTaskCustomPayload, ESPMode::ThreadSafe>();
	FVoxelFineHandoffTransaction Transaction;
	FString Error;
	if (!TestTrue(TEXT("Known empty geometry prepares without an Actor"),
		Handoff.Prepare(MakeArrayView(&Change, 1), 1, Transaction, Error))) return false;
	TestNull(TEXT("Preparation is not presented coverage"), Handoff.FindPresentedAtom(Change.Section));
	const auto Current = [](const FIntVector&, const FVoxelFineOwnerEvidence&) { return true; };
	TSharedPtr<const FVoxelFineFrozenHandoff, ESPMode::ThreadSafe> Frozen;
	if (!TestTrue(TEXT("A complete empty proof can seal"), Handoff.SealTransaction(Transaction.Id, {1, 1, 1},
		Change.After.GeometryLease, Current, Current,
		[](const FVoxelFineHandoffTransaction&, const FVoxelTaskCustomPayload&) { return true; }, Frozen, Error))) return false;
	TestNull(TEXT("Sealing still waits for the actual receipt"), Handoff.FindPresentedAtom(Change.Section));
	TOptional<FVoxelFineHandoffTransaction> Canceled;
	TestTrue(TEXT("Late cancellation retains the sealed historical fact"),
		Handoff.RequestCancel(Transaction.Id, Canceled) == EVoxelFineCancelDisposition::RecordedAfterSeal);
	FVoxelFineRenderReceipt Receipt;
	Receipt.TransactionId = Transaction.Id;
	Receipt.Stamp = Frozen->Stamp;
	Receipt.Atoms.Add({Change.Section, Change.After.Owner, 1, Change.After.Content, true});
	TArray<FVoxelFinePresentedAtom> Retiring;
	TestTrue(TEXT("Only the receipt installs empty coverage"),
		Handoff.CommitRenderReceipt(Receipt, Retiring, Error) == EVoxelFineReceiptDisposition::Applied);
	const auto* Presented = Handoff.FindPresentedAtom(Change.Section);
	if (!TestNotNull(TEXT("Committed empty ownership remains available"), Presented)) return false;
	TestTrue(TEXT("No render Actor is needed for an explicit empty proof"), Presented->Evidence.bKnownEmpty);
	Change.Before = Presented->Evidence.Owner;
	Change.BeforeGeneration = Presented->OwnerGeneration;
	Change.After.bKnownEmpty = Change.After.bGeometryReady = false;
	Change.After.Content.GeometryIdentity = FGuid::NewGuid();
	TestFalse(TEXT("Missing replacement geometry cannot impersonate empty terrain"),
		Handoff.Prepare(MakeArrayView(&Change, 1), 2, Transaction, Error));
	TestTrue(TEXT("Failed replacement preserves previously presented coverage"),
		Handoff.FindPresentedAtom(Change.Section)->Evidence.bKnownEmpty);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelKnownSchedulerProgressIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.SchedulerProgressIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelKnownSchedulerProgressIntegrationTest::RunTest(const FString& InParameters)
{
	for (const int32 WorkerCount : {1, 4})
	{
		FVoxelTerrainFrameContext Frame;
		FString FrameError;
		if (!TestTrue(TEXT("实际帧预算初始化"), Frame.Configure({}, FrameError))) return false;
		FVoxelTaskScheduler Scheduler(true);
		Scheduler.BindFrameContext(Frame);
		FVoxelTaskBudget Budget;
		Budget.MaxConcurrentTasks = WorkerCount;
		Budget.MaxConcurrentCoarseTerrainTasks = WorkerCount;
		Budget.MaxConcurrentSurfaceTasks = WorkerCount;
		Budget.MaxConcurrentMacroTasks = WorkerCount;
		Scheduler.SetBudget(Budget);
		Scheduler.SetObservedTerrainStage(0);
		TAtomic<bool> Release(false);
		const EVoxelTaskKind Kinds[] = {EVoxelTaskKind::BuildCollision, EVoxelTaskKind::GenerateVoxelProxy,
			EVoxelTaskKind::GenerateSurface, EVoxelTaskKind::GenerateMacro, EVoxelTaskKind::BuildViewCoverage,
			EVoxelTaskKind::BuildSurface, EVoxelTaskKind::BuildMacro};
		int32 Completed[UE_ARRAY_COUNT(Kinds)] = {};
		uint64 NextCoverage = 100;
		auto Enqueue = [&](const EVoxelTaskKind Kind, const uint64 Token)
		{
			FVoxelTaskRequest Request;
			Request.Kind = Kind;
			Request.Stamp.Token = Token;
			Request.TerrainStage = Kind == EVoxelTaskKind::BuildViewCoverage ? 1 : INDEX_NONE;
			Request.WorkClass = Kind == EVoxelTaskKind::BuildCollision ? EVoxelWorkClass::Critical : EVoxelWorkClass::Visible;
			Request.ReservedBytes = 1024;
			Request.Execute = [&Release](const TAtomic<bool>& Cancel)
			{
				while (!Release.Load() && !Cancel.Load()) FPlatformProcess::Sleep(0.001f);
				FVoxelTaskResult Result;
				Result.bSuccess = !Cancel.Load();
				return Result;
			};
			return Scheduler.Enqueue(MoveTemp(Request));
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Kinds); ++Index)
		{
			TestTrue(TEXT("Real scheduler accepts each service lane"), Enqueue(Kinds[Index], Index + 1));
		}
		TestFalse(TEXT("Duplicate acceptance cannot leak producer reservations"), Enqueue(Kinds[1], 2));
		Release.Store(true);
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		uint64 FrameSerial = 0;
		while (FPlatformTime::Seconds() < Deadline)
		{
			if (!Frame.Begin(1, ++FrameSerial, FPlatformTime::Seconds(), {})) return false;
			Enqueue(EVoxelTaskKind::BuildViewCoverage, ++NextCoverage);
			Scheduler.Tick([&](FVoxelTaskResult&& Result)
			{
				if (Result.Stamp.Token <= UE_ARRAY_COUNT(Kinds) && Result.bSuccess && !Result.bCanceled)
				{
					++Completed[Result.Stamp.Token - 1];
				}
			}, 8.0);
			Frame.End(FPlatformTime::Seconds());
			bool bFinished = true;
			for (const int32 Count : Completed) bFinished &= Count == 1;
			if (bFinished) break;
			FPlatformProcess::Sleep(0.001f);
		}
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Kinds); ++Index)
		{
			TestEqual(*FString::Printf(TEXT("连续Coverage下实际lane完成: workers=%d kind=%d"), WorkerCount, int32(Kinds[Index])), Completed[Index], 1);
		}
		Scheduler.SetObservedTerrainStage(0);
		Scheduler.StopAndJoin();
	}
	return true;
}


namespace
{
	double TerrainMeshArea(const FVoxelSectionMeshResult& Mesh)
	{
		double Area = 0.0;
		for (const auto& Batch : Mesh.Batches)
			for (int32 Index = 0; Index < Batch.Mesh.Triangles.Num(); Index += 3)
			{
				const FVector A = Batch.Mesh.Vertices[Batch.Mesh.Triangles[Index]];
				const FVector B = Batch.Mesh.Vertices[Batch.Mesh.Triangles[Index + 1]];
				const FVector C = Batch.Mesh.Vertices[Batch.Mesh.Triangles[Index + 2]];
				Area += FVector::CrossProduct(B - A, C - A).Length() * 0.5;
			}
		return Area;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelResumablePageIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.ResumablePageIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResumablePageIntegrationTest::RunTest(const FString& InParameters)
{
	FVoxelTaskScheduler Scheduler(true);
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 1;
	Scheduler.SetBudget(Budget);
	TAtomic<int32> Executions(0);
	FVoxelTaskRequest Request;
	Request.Kind = EVoxelTaskKind::BuildVoxelProxy;
	Request.WorkClass = EVoxelWorkClass::Visible;
	Request.Stamp.Token = 701;
	Request.InputBytes = 1024;
	Request.ReservedBytes = 1024;
	Request.bResumable = true;
	Request.Execute = [&Executions](const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		Result.bSuccess = !Cancel.Load();
		Result.bContinue = ++Executions < 8;
		return Result;
	};
	TestTrue(TEXT("Logical page admitted"), Scheduler.Enqueue(MoveTemp(Request)));
	int32 Applied = 0;
	const double Deadline = FPlatformTime::Seconds() + 5.0;
	while (Scheduler.ActiveCount() && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([&](FVoxelTaskResult&& Result)
		{
			TestFalse(TEXT("Partial shard is never applied"), Result.bContinue);
			++Applied;
		});
		FPlatformProcess::Sleep(0.001f);
	}
	TestEqual(TEXT("All eight original-queue windows execute"), Executions.Load(), 8);
	TestEqual(TEXT("The completed logical page applies exactly once"), Applied, 1);
	TestEqual(TEXT("Completion releases logical ownership"), Scheduler.ActiveCount(), 0);
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelResumableFineSameBandFairnessTest,
	"WHFramework.Voxel.KnownRequirements.ResumableFineSameBandFairness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResumableFineSameBandFairnessTest::RunTest(const FString& InParameters)
{
	FVoxelTaskScheduler Scheduler(true);
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 1;
	Scheduler.SetBudget(Budget);
	TAtomic<int32> FineExecutions(0);

	FVoxelTaskRequest Fine;
	Fine.Kind = EVoxelTaskKind::BuildFineShard;
	Fine.WorkClass = EVoxelWorkClass::Visible;
	Fine.Stamp.Token = 801;
	Fine.DistanceScore = 100.0;
	Fine.InputBytes = 1024;
	Fine.ReservedBytes = 1024;
	Fine.bResumable = true;
	Fine.Execute = [&FineExecutions](const TAtomic<bool>& Cancel)
	{
		++FineExecutions;
		FVoxelTaskResult Result;
		Result.bSuccess = !Cancel.Load();
		Result.bContinue = Result.bSuccess;
		return Result;
	};
	if (!TestTrue(TEXT("持续续跑的 Fine 任务入队"), Scheduler.Enqueue(MoveTemp(Fine)))) return false;

	FVoxelTaskRequest Exact;
	Exact.Kind = EVoxelTaskKind::GenerateExactBase;
	Exact.WorkClass = EVoxelWorkClass::Visible;
	Exact.Stamp.Token = 802;
	Exact.DistanceScore = 200.0;
	Exact.InputBytes = 1024;
	Exact.ReservedBytes = 1024;
	Exact.Execute = [](const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		Result.bSuccess = !Cancel.Load();
		return Result;
	};
	if (!TestTrue(TEXT("同服务带的精确地形任务入队"), Scheduler.Enqueue(MoveTemp(Exact))))
	{
		Scheduler.StopAndJoin();
		return false;
	}

	bool bExactApplied = false;
	const double Deadline = FPlatformTime::Seconds() + 5.0;
	while (!bExactApplied && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([&bExactApplied](FVoxelTaskResult&& Result)
		{
			if (Result.Stamp.Token == 802 && Result.bSuccess) bExactApplied = true;
		});
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("Fine 至少执行一次切片"), FineExecutions.Load() > 0);
	TestTrue(TEXT("持续 Fine 续跑期间，同服务带 Exact 也得到执行"), bExactApplied);
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHeightfieldWindowIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.HeightfieldWindowIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHeightfieldWindowIntegrationTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	FVoxelRegistrySnapshot RegistryValue = *VoxelTest::MakeRegistry();
	RegistryValue.Definitions[Config->Water.TypeId].RenderGroup = EVoxelRenderGroup::Water;
	const auto Registry = MakeShared<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe>(MoveTemp(RegistryValue));
	FVoxelSurfaceTileData Data;
	Data.Key = {FIntPoint(-1, 2), 2};
	Data.Geometry = {{EVoxelTerrainRepresentation::Surface, FIntVector(-1, 2, 0), 2}, 1, -64, 128, 17, 23};
	Data.Side = 65;
	Data.Step = 1;
	const int32 Count = Data.Side * Data.Side;
	Data.GroundZ.SetNum(Count);
	Data.SurfaceStates.Init(Config->Grass, Count);
	Data.WaterZ.Init(MIN_int32, Count);
	Data.Flags.Init(0, Count);
	for (int32 Y = 0; Y < Data.Side; ++Y)
		for (int32 X = 0; X < Data.Side; ++X)
		{
			const int32 Index = X + Y * Data.Side;
			Data.GroundZ[Index] = X < 32 ? 7 : 12;
			if (Y < 20) { Data.WaterZ[Index] = 15; Data.Flags[Index] = VoxelSurface_Lake; }
		}
	FString Error;
	FVoxelWaterSurfaceTileData Water;
	if (!FVoxelWaterViewBuilder().Build(Data, Water, Error)) { AddError(Error); return false; }
	FVoxelSectionMeshResult Whole, WaterMesh, Sliced;
	if (!FVoxelHeightfieldMesher::BuildBlockyTerrain(Data.Side, Data.Step, Data.GroundZ,
		Data.SurfaceStates, *Config, *Registry, Whole, Error) ||
		!FVoxelHeightfieldMesher::BuildWater(Water, *Registry, WaterMesh, Error)) { AddError(Error); return false; }
	Whole.Batches.Append(MoveTemp(WaterMesh.Batches));
	FVoxelHeightfieldPageMesh Page;
	if (!Page.Initialize(Data.Geometry, Error)) { AddError(Error); return false; }
	bool Complete = false;
	int32 Windows = 0;
	while (!Complete && Windows < 20)
	{
		if (!Page.Advance(Data, Water, *Config, *Registry, 32.0, Sliced, Complete, Error, nullptr))
		{ AddError(Error); return false; }
		++Windows;
		if (!Complete) TestTrue(TEXT("Intermediate window never exposes a parent mesh"), Sliced.Batches.IsEmpty());
	}
	TestTrue(TEXT("All windows complete"), Complete);
	TestTrue(TEXT("Several independent windows execute"), Windows > 4);
	TestTrue(TEXT("Cross-window height steps and shoreline preserve exact surface area"),
		FMath::IsNearlyEqual(TerrainMeshArea(Whole), TerrainMeshArea(Sliced), 0.001));
	TestTrue(TEXT("Completed mesh carries worker validation"), Sliced.bValidated);
	FVoxelHeightfieldPageMesh Counter;
	if (!Counter.Initialize(Data.Geometry, Error, 0, true)) { AddError(Error); return false; }
	FVoxelSectionMeshResult Counted;
	bool CountComplete = false;
	for (int32 Index = 0; Index < 20 && !CountComplete; ++Index)
		if (!Counter.Advance(Data, Water, *Config, *Registry, 32.0, Counted, CountComplete, Error, nullptr))
		{ AddError(Error); return false; }
	TestTrue(TEXT("Shoreline preflight completes without vertex allocations"), CountComplete && Counted.Batches.IsEmpty());
	TestTrue(TEXT("Preflight covers allocated terrain, water and shoreline capacity"), Counter.GetEstimatedMeshBytes() >= Sliced.AllocatedBytes());
	FVoxelSurfaceTileData Changed = Data;
	Changed.Geometry.SampleStepCells = 2;
	bool Ignored = false;
	TestFalse(TEXT("Same spatial key cannot reuse a different sample geometry"),
		Page.Advance(Changed, Water, *Config, *Registry, 32.0, Sliced, Ignored, Error, nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHeightfieldCapacityIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.HeightfieldCapacityIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHeightfieldCapacityIntegrationTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	FVoxelRegistrySnapshot RegistryValue = *VoxelTest::MakeRegistry();
	RegistryValue.Definitions[Config->Water.TypeId].RenderGroup = EVoxelRenderGroup::Water;
	const auto Registry = MakeShared<const FVoxelRegistrySnapshot, ESPMode::ThreadSafe>(MoveTemp(RegistryValue));
	FVoxelSurfaceTileData Data;
	Data.Key = {FIntPoint::ZeroValue, 4};
	Data.Geometry = {{EVoxelTerrainRepresentation::Surface, FIntVector::ZeroValue, 4}, 1, -64, 128, 17, 23};
	Data.Side = 257;
	Data.Step = 1;
	const int32 Count = Data.Side * Data.Side;
	Data.GroundZ.Init(8, Count);
	Data.SurfaceStates.Init(Config->Grass, Count);
	Data.WaterZ.Init(MIN_int32, Count);
	Data.Flags.Init(0, Count);
	FString Error;
	FVoxelWaterSurfaceTileData Water;
	if (!FVoxelWaterViewBuilder().Build(Data, Water, Error)) { AddError(Error); return false; }
	FVoxelHeightfieldPageMesh Counter;
	if (!Counter.Initialize(Data.Geometry, Error, 0, true)) { AddError(Error); return false; }
	FVoxelSectionMeshResult Mesh;
	bool Complete = false;
	for (int32 Index = 0; Index < 100 && !Complete; ++Index)
		if (!Counter.Advance(Data, Water, *Config, *Registry, 32.0, Mesh, Complete, Error, nullptr))
		{ AddError(Error); return false; }
	TestTrue(TEXT("Large legal page preflight completes"), Complete);
	const uint64 Capacity = Counter.GetEstimatedMeshBytes();
	TestTrue(TEXT("Fixture exceeds the former fixed mesh capacity"), Capacity > 12ull * 1024ull * 1024ull);
	FVoxelHeightfieldPageMesh Page;
	if (!Page.Initialize(Data.Geometry, Error, Capacity)) { AddError(Error); return false; }
	Complete = false;
	for (int32 Index = 0; Index < 100 && !Complete; ++Index)
		if (!Page.Advance(Data, Water, *Config, *Registry, 32.0, Mesh, Complete, Error, nullptr))
		{ AddError(Error); return false; }
	TestTrue(TEXT("Large parent publishes only after all execution windows complete"), Complete && Mesh.bValidated);
	TestTrue(TEXT("Preflight bounds the real allocated mesh"), Mesh.AllocatedBytes() <= Capacity);
	TestTrue(TEXT("Complete parent retains its full spatial surface"),
		FMath::IsNearlyEqual(TerrainMeshArea(Mesh), 256.0 * 256.0, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelProxyWindowIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.ProxyWindowIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelProxyWindowIntegrationTest::RunTest(const FString& InParameters)
{
	const auto Registry = VoxelTest::MakeRegistry();
	FVoxelShapeRegistry Shapes;
	Shapes.BuildDefaults();
	FVoxelVoxelProxyData Data;
	Data.Key = {FIntVector::ZeroValue, 2};
	Data.Geometry = {{EVoxelTerrainRepresentation::VoxelProxy, FIntVector::ZeroValue, 2}, 2, -64, 128, 17, 23};
	Data.GridSide = 32;
	Data.Cells.Init(FVoxelBlockState{}, 32 * 32 * 32);
	for (int32 Z = 0; Z < 19; ++Z)
		for (int32 Y = 0; Y < 32; ++Y)
			for (int32 X = 0; X < 32; ++X) Data.Cells[X + 32 * (Y + 32 * Z)] = FVoxelBlockState(1, 0);
	FVoxelSectionSnapshot Snapshot;
	Snapshot.GridSide = 32;
	for (int32 Index = 0; Index < Data.Cells.Num(); ++Index) Snapshot.Blocks.Add(Data.Cells[Index].Pack());
	for (int32 Face = 0; Face < 6; ++Face)
	{
		Data.Known[Face] = Snapshot.Known[Face] = true;
		Data.Halo[Face].Init(FVoxelBlockState{}, 32 * 32);
		Snapshot.Halo[Face].Init(0, 32 * 32);
	}
	FVoxelSectionMeshResult Whole, Sliced;
	TestTrue(TEXT("Reference full parent builds"), FVoxelSectionMesher::Build(Snapshot, *Registry, Shapes, Whole));
	FVoxelProxyPageMesh Page;
	FString Error;
	if (!Page.Initialize(Data.Geometry, Error)) { AddError(Error); return false; }
	bool Complete = false;
	for (int32 Window = 0; Window < 12 && !Complete; ++Window)
		if (!Page.Advance(Data, *Registry, Shapes, nullptr, 1.0, Sliced, Complete, Error, nullptr))
		{ AddError(Error); return false; }
	TestTrue(TEXT("Parent mesh finishes after bounded windows"), Complete);
	TestTrue(TEXT("Reference includes actual block surfaces"), TerrainMeshArea(Whole) > 0.0);
	TestTrue(TEXT("Internal shard halo prevents extra faces or holes"),
		FMath::IsNearlyEqual(TerrainMeshArea(Whole), TerrainMeshArea(Sliced), 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelStageArrayIntegrationTest,
	"WHFramework.Voxel.KnownRequirements.StageArrayIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStageArrayIntegrationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;

	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -64;
	Manifest.Settings.MaxZ = 128;

	FVoxelViewSettings Settings;
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Center = FIntVector(8);
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual | EVoxelStreamingCapability::WorldVisual;
	Source.bInheritWorldView = false;
	Source.ExactRadius = -1;
	Source.View.FineRadiusCells = 16;
	Source.View.FineVerticalHalfExtentCells = 16;
	Source.View.VoxelProxyRadiusCells = 64;
	Source.View.VoxelProxyVerticalHalfExtentCells = 64;
	Source.View.SurfaceRadiusCells = 128;
	Source.View.MacroRadiusCells = 256;

	const auto Interest = FVoxelInterestManager().Compute(MakeArrayView(&Source, 1), Manifest, Settings);
	if (!Interest.PlanningError.IsEmpty())
	{
		AddError(Interest.PlanningError);
		return false;
	}

	const auto* Accepted = Interest.Sources.Find(Source.Id);
	TestNotNull(TEXT("Effective source is retained"), Accepted);
	if (!Accepted)
	{
		return false;
	}

	const auto MinimumSide = [&Interest](const EVoxelTerrainRepresentation Representation)
	{
		int32 Result = MAX_int32;
		for (const auto& Pair : Interest.Geometry)
		{
			if (Pair.Key.Representation == Representation)
			{
				Result = FMath::Min(Result, static_cast<int32>(Pair.Key.SideCells()));
			}
		}
		return Result == MAX_int32 ? 16 : Result;
	};

	TestEqual(TEXT("Proxy first side reports the actual plan"), Accepted->FirstSideCells[0], MinimumSide(EVoxelTerrainRepresentation::VoxelProxy));
	TestEqual(TEXT("Surface first side reports the actual plan"), Accepted->FirstSideCells[1], MinimumSide(EVoxelTerrainRepresentation::Surface));
	TestEqual(TEXT("Macro first side reports the actual plan"), Accepted->FirstSideCells[2], MinimumSide(EVoxelTerrainRepresentation::Macro));

	for (const int32 Side : Accepted->FirstSideCells)
	{
		TestTrue(TEXT("Recorded page side is a valid power-of-two side"), Side >= 16 && FMath::IsPowerOfTwo(static_cast<uint32>(Side)));
	}

	return true;
}

#endif
