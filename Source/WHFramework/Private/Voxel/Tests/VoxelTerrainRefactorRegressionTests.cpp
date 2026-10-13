#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Main/MainModule.h"
#include "HAL/PlatformProcess.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Rendering/VoxelCoverage.h"
#include "Voxel/Rendering/VoxelPublishBudget.h"
#include "Voxel/Rendering/VoxelTerrainStagePlanner.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Streaming/VoxelEmergeManager.h"
#include "Voxel/Save/VoxelRegionStore.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/VoxelModule.h"
#include "VoxelTestUtilities.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelInterestFineLayoutRevisionTest, "WHFramework.Voxel.Streaming.FineLayoutRevision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelInterestFineLayoutRevisionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;

	FVoxelInterestRuntime Runtime;
	FVoxelWorldManifest Manifest;
	FVoxelViewSettings Settings;
	Settings.Fine.MaximumPageLevel = 2;

	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual | EVoxelStreamingCapability::WorldVisual;
	Source.View.FineRadiusCells = 512;
	Source.View.FineLevel0RadiusCells = 32;
	Source.View.FineVerticalHalfExtentCells = 128;
	Source.Center = FIntVector(8, 8, 8);
	Source.SchedulingPriority = 0;

	FVoxelInterestSet First;
	FVoxelInterestDelta Delta;
	bool bSnapshot = false;
	const auto Advance = [&](FVoxelInterestSet& OutInterest)
	{
		bool bChanged = false;
		for (int32 Step = 0; Step < 512; ++Step)
		{
			bChanged |= Runtime.Update(MakeArrayView(&Source, 1), Manifest, Settings, OutInterest, Delta, bSnapshot);
			if (bSnapshot || !Runtime.GetPlanningError().IsEmpty() || !Runtime.GetCoarsePlanningPending()) break;
		}
		if (!bSnapshot) AddError(FString::Printf(TEXT("Layout fixture planning failed: %s"), *Runtime.GetPlanningError()));
		return bChanged;
	};
	TestTrue(TEXT("Initial interest changes"), Advance(First));
	if (!TestTrue(TEXT("Initial snapshot exists"), bSnapshot) || !TestTrue(TEXT("Initial Fine set exists"), First.FineSections.IsValid()) ||
		!TestTrue(TEXT("Initial Fine page plan exists"), First.FinePagePlan.IsValid()))
	{
		return false;
	}

	const TSet<FIntVector> FirstFine = *First.FineSections;
	const TSet<FVoxelFinePageKey> FirstPages = First.FinePagePlan->Pages;

	// 只改变 Page 最大合并级，不改变 Source 中心、FineRadius、ExactRadius、
	// CollisionRadius 或任何需求范围。旧实现会得到新的 FinePagePlan，
	// 但因为 logical demand/geometry/priorities 都相同而返回 bChanged=false。
	Settings.Fine.MaximumPageLevel = 3;

	FVoxelInterestSet Second;
	Delta.Reset();
	bSnapshot = false;
	const bool bChanged = Advance(Second);

	if (!TestTrue(TEXT("Layout-only policy update produces accepted change"), bChanged && bSnapshot) ||
		!TestTrue(TEXT("Updated Fine set exists"), Second.FineSections.IsValid()) ||
		!TestTrue(TEXT("Updated Fine page plan exists"), Second.FinePagePlan.IsValid()))
	{
		return false;
	}

	TestTrue(TEXT("Fine logical demand is unchanged"),
		Second.FineSections->Num() == FirstFine.Num() && Second.FineSections->Includes(FirstFine) && FirstFine.Includes(*Second.FineSections));
	TestTrue(TEXT("Visual partition marks layout-only change"), Delta.bVisualPartitionChanged);
	TestFalse(TEXT("Fine page ownership is not silently reused"), Second.FinePagePlan->Pages.Num() == FirstPages.Num() &&
																	  Second.FinePagePlan->Pages.Includes(FirstPages) &&
																	  FirstPages.Includes(Second.FinePagePlan->Pages));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFinePageObsoleteDirtyCleanupTest, "WHFramework.Voxel.Rendering.FinePageObsoleteDirtyCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFinePageObsoleteDirtyCleanupTest::RunTest(const FString& InParameters)
{
	(void)InParameters;

	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler(true);
	FVoxelViewManager Manager(*Module, Scheduler, 1);

	const FVoxelFinePageKey Obsolete{FIntVector(5, 5, 0), 2};
	Manager.DirtyFinePages.Add(Obsolete);
	const FVoxelFineShardKey Retained{FIntVector::ZeroValue, 32};
	FVoxelFineMeshInput Input;
	Input.WorldEpoch = 1;
	Input.Incarnation = 7;
	Input.GeneratorIdentity = 11;
	Input.Shard = Retained;
	FString Error;
	auto& Slot = Manager.FineShardSlots.Add(Retained);
	Slot.OrderIndex = Manager.FineShardSlotOrder.Add(Retained);
	Manager.FineShardOwnershipAtomCapacity = 8;
	if (!Slot.Build.Initialize(Input, Error)) return false;
	const auto Attempt = Slot.Build.BeginAttempt();

	Manager.GetInputRoot().FinePagePlan.Pages.Reset();
	Manager.RetireFinePageLayoutState(Obsolete);

	TestFalse(TEXT("Obsolete page leaves dirty workset"), Manager.DirtyFinePages.Contains(Obsolete));
	TestTrue(TEXT("Page layout retirement preserves the retained canonical Shard owner"), Manager.FineShardSlots.Contains(Retained));
	TestTrue(TEXT("Page regrouping cannot cancel a retained Shard content attempt"), Slot.Build.GetPendingAttempt().IsSet() && Slot.Build.GetPendingAttempt().GetValue() == Attempt);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainSampleWorkBoundTest, "WHFramework.Voxel.Streaming.TerrainSampleWorkBound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainSampleWorkBoundTest::RunTest(const FString& InParameters)
{
	(void)InParameters;

	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.SchedulingPriority = 0;
	Source.Center = FIntVector::ZeroValue;
	Source.VerticalFovDegrees = 60.0f;
	Source.ViewportHeightPixels = 1440;

	FVoxelTerrainStageSource StageSource{Source, 2048, 256, 0};

	FVoxelTerrainStagePlan Plan;
	FString Error;
	TestTrue(TEXT("Proxy plan builds"), FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy, MakeArrayView(&StageSource, 1), 7, 64, -512,
											512, 8.0, 1, 1, Plan, Error, nullptr));

	FVoxelCoverageIndex FineCoverage;
	auto Fine = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
	Fine->Geometry = {{EVoxelTerrainRepresentation::Fine, FIntVector::ZeroValue, 0}, 1, -512, 512, 1, 1};
	Fine->Geometry.Key.TryBounds(-512, 512, Fine->Min, Fine->Max);
	TestTrue(TEXT("Fine fixture is indexed"), FineCoverage.Upsert(Fine, Error));
	TestTrue(TEXT("Proxy plan uses a concrete Fine predecessor"), FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy,
																	  MakeArrayView(&StageSource, 1), 7, 64, -512, 512, 8.0, 1, 1, Plan, Error, &FineCoverage));
	bool bHasStepOneOutsideFine = false;
	for (const auto& Pair : Plan.Pages)
	{
		const FVoxelTerrainGeometry& Geometry = Pair.Value;
		TestTrue(TEXT("Every Proxy page obeys max samples per axis"), Geometry.Key.SideCells() / Geometry.SampleStepCells <= 64);
		FIntVector Min, Max;
		Geometry.Key.TryBounds(-512, 512, Min, Max);
		const bool bOutsideFine =
			Max.X <= Fine->Min.X || Min.X >= Fine->Max.X || Max.Y <= Fine->Min.Y || Min.Y >= Fine->Max.Y || Max.Z <= Fine->Min.Z || Min.Z >= Fine->Max.Z;
		bHasStepOneOutsideFine |= Geometry.SampleStepCells == 1 && bOutsideFine;
		TestTrue(TEXT("Proxy identity never aliases Fine L0"), Geometry.Key.Level >= 1);
	}

	TestFalse(TEXT("Proxy cannot use Fine's 25cm sample step outside Fine"), bHasStepOneOutsideFine);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineShardAttemptIsolationTest, "WHFramework.Voxel.Rendering.FineShardAttemptIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelFineShardAttemptIsolationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	Module->Epoch = 11;
	Module->Runtime = MakeUnique<FVoxelWorldRuntime>(11, true, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	FVoxelTaskScheduler Scheduler(true);
	FVoxelViewManager Manager(*Module, Scheduler, 11);
	const FVoxelFinePageKey Other{FIntVector(8, 0, 0), 1};
	const FVoxelFineShardKey Key{FIntVector::ZeroValue, 32};
	FVoxelFineMeshInput Input;
	Input.WorldEpoch = 11;
	Input.Incarnation = 7;
	Input.GeneratorIdentity = 11;
	Input.Shard = Key;
	FString Error;
	auto& Slot = Manager.FineShardSlots.Add(Key);
	Slot.OrderIndex = Manager.FineShardSlotOrder.Add(Key);
	Manager.FineShardOwnershipAtomCapacity = 8;
	if (!Slot.Build.Initialize(Input, Error)) return false;
	const auto Old = Slot.Build.BeginAttempt();
	const FGuid OwnerId = FGuid::NewGuid();
	const FVoxelGenerationConsumerKey OldConsumer{EVoxelGenerationConsumerKind::FineShard, 11, OwnerId, 7,
		EVoxelGenerationSpatialKind::Bounds, Key.Min, Key.Bounds().Max, Old.Serial};
	Slot.Build.NotifyMeshPolicyChanged(2, 3);
	const auto Current = Slot.Build.BeginAttempt();
	Slot.Consumer = {EVoxelGenerationConsumerKind::FineShard, 11, OwnerId, 7,
		EVoxelGenerationSpatialKind::Bounds, Key.Min, Key.Bounds().Max, Current.Serial};
	Slot.SubmittedStamp.WorldEpoch = 11;
	Slot.SubmittedStamp.Token = Current.Serial;
	Slot.bSubmitted = true;
	const auto Resources = Scheduler.GetTerrainResources();
	const int32 OriginalLeaseCount = Resources->Snapshot().LeaseCount;
	const auto RetainSamplingProgress = [&]()
	{
		if (Resources->TryAcquire(Slot.Consumer, EVoxelTerrainResourcePool::Input, 65536,
			Slot.SamplesReadyLease) != EVoxelTerrainResourceAdmission::Granted) return false;
		Slot.SamplesReady = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(true);
		return true;
	};
	if (!TestTrue(TEXT("采样进度持有原输入租约"), RetainSamplingProgress())) return false;
	const uint64 OriginalProgressSerial = Slot.SamplesReadyLease->Serial();
	Manager.InvalidateFinePageLayout(Other);
	const uint64 ContentVersion = Slot.Build.GetInput().MeshRevision;
	const uint64 TopologyVersion = Slot.Build.GetTopologyRevision();
	Manager.OnOwnershipChanged(Key);
	TestEqual(TEXT("Actual local ownership event advances only topology"), Slot.Build.GetTopologyRevision(), TopologyVersion + 1);
	TestEqual(TEXT("Actual ownership event preserves the content version"), Slot.Build.GetInput().MeshRevision, ContentVersion);
	TestTrue(TEXT("Unrelated page layout preserves this real Shard attempt"), Slot.Build.IsCurrent(Current));
	FVoxelTaskResult Late;
	Late.Stamp.WorldEpoch = 11;
	Late.Stamp.Token = Old.Serial;
	Late.bCanceled = true;
	Manager.ApplyFineShardResult(Key, Old, OldConsumer, MoveTemp(Late));
	TestTrue(TEXT("Late original-consumer callback cannot clear the real new Shard attempt"),
		Slot.bSubmitted && Slot.Build.GetPendingAttempt().IsSet() && Slot.Build.GetPendingAttempt().GetValue() == Current);
	TestTrue(TEXT("迟到回调保留新采样进度与原租约"), Slot.SamplesReady && Slot.SamplesReady->Load() &&
		Slot.SamplesReadyLease && Slot.SamplesReadyLease->Serial() == OriginalProgressSerial);
	const auto CanceledConsumer = Slot.Consumer;
	FVoxelTaskResult Cancelled;
	Cancelled.Stamp = Slot.SubmittedStamp;
	Cancelled.bCanceled = true;
	Cancelled.InputResourceLease = Slot.SamplesReadyLease;
	Manager.ApplyFineShardResult(Key, Current, Slot.Consumer, MoveTemp(Cancelled));
	TestFalse(TEXT("Matching native cancellation releases its original attempt"), Slot.Build.GetPendingAttempt().IsSet());
	TestTrue(TEXT("Matching native cancellation permits retained demand retry"), Slot.Build.GetFailure().IsEmpty() && !Slot.bSubmitted);
	TestTrue(TEXT("匹配完成撤销槽对整个工作输入的额外持有"), !Slot.SamplesReady && !Slot.SamplesReadyLease);
	TestEqual(TEXT("原结果仍存活时输入没有提前退款"), Resources->Snapshot().LeaseCount, OriginalLeaseCount + 1);
	Cancelled.InputResourceLease.Reset();
	TestEqual(TEXT("原结果退出后工作输入真实归还"), Resources->Snapshot().LeaseCount, OriginalLeaseCount);
	const auto FailedAttempt = Slot.Build.BeginAttempt();
	Slot.Consumer.RequestSerial = FailedAttempt.Serial;
	Slot.SubmittedStamp.Token = FailedAttempt.Serial;
	Slot.bSubmitted = true;
	if (!TestTrue(TEXT("同内容重试取得独立原输入授予"), RetainSamplingProgress())) return false;
	const uint64 RetryProgressSerial = Slot.SamplesReadyLease->Serial();
	FVoxelTaskResult LateCancellation;
	LateCancellation.Stamp = Cancelled.Stamp;
	LateCancellation.bCanceled = true;
	Manager.ApplyFineShardResult(Key, Current, CanceledConsumer, MoveTemp(LateCancellation));
	TestTrue(TEXT("相同内容的旧取消不能撤销新序号"), Slot.bSubmitted &&
		Slot.Build.GetPendingAttempt().IsSet() && Slot.Build.GetPendingAttempt().GetValue() == FailedAttempt &&
		Slot.SamplesReady && Slot.SamplesReadyLease && Slot.SamplesReadyLease->Serial() == RetryProgressSerial);
	FVoxelTaskResult Failed;
	Failed.Stamp = Slot.SubmittedStamp;
	Failed.Error = TEXT("Rejected mesh fixture");
	Failed.InputResourceLease = Slot.SamplesReadyLease;
	AddExpectedError(TEXT("Fine shard build failed"), EAutomationExpectedErrorFlags::Contains, 1);
	Manager.ApplyFineShardResult(Key, FailedAttempt, Slot.Consumer, MoveTemp(Failed));
	TestTrue(TEXT("Native Shard failure remains an explicit terminal input result"), !Slot.Build.GetFailure().IsEmpty());
	TestTrue(TEXT("失败完成也撤销采样进度持有"), !Slot.SamplesReady && !Slot.SamplesReadyLease);
	Failed.InputResourceLease.Reset();
	TestEqual(TEXT("失败结果退出后不遗留工作输入授予"), Resources->Snapshot().LeaseCount, OriginalLeaseCount);
	TestEqual(TEXT("Failure remains available to acceptance diagnostics"), Manager.GetTerrainWorkMetrics().Failed, uint64(1));
	Manager.InvalidateFinePageLayout(Other);
	TestTrue(TEXT("Layout-only events do not clear a real content failure"), !Slot.Build.GetFailure().IsEmpty());
	Slot.Build.NotifyDataChanged(1, 1);
	TestTrue(TEXT("A real content change can recover from the native failure"), Slot.Build.GetFailure().IsEmpty());
	TestEqual(TEXT("Late and canceled results are distinguished"), Manager.GetTerrainWorkMetrics().Stale, uint64(2));
	TestEqual(TEXT("Matching native cancellation is accounted once"), Manager.GetTerrainWorkMetrics().Canceled, uint64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSourceAcceptanceBarrierTest, "WHFramework.Voxel.Streaming.SourceAcceptanceBarrier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelSourceAcceptanceBarrierTest::RunTest(const FString& InParameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Source acceptance fixture has a World"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	AMainModule* Owner = World->SpawnActor<AMainModule>();
	if (!TestNotNull(TEXT("Source acceptance fixture has its real module owner"), Owner)) return false;
	UVoxelModule* Module = NewObject<UVoxelModule>(Owner);
	if (!TestEqual(TEXT("Source acceptance module resolves its owning World"), Module->GetWorld(), World)) return false;
	Module->Epoch = 11;
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Center = FIntVector(1, 2, 3);
	Source.Normalize();
	auto& Registered = Module->Sources.FindOrAdd(Source.Id);
	Registered.Owner = Module;
	Registered.Value = Source;
	Registered.PlannedValue = Source;
	FVoxelInterestSet Accepted;
	Accepted.Sources.FindOrAdd(Source.Id).Source = Source;
	Module->CurrentInterest = MakeShared<const FVoxelInterestSet, ESPMode::ThreadSafe>(Accepted);
	TestTrue(TEXT("Accepted source center passes"), Module->IsSourceAdmittedAt(Source.Id, Source.Center, 11));
	const FIntVector Moved = Source.Center + FIntVector(1, 0, 0);
	FVoxelStreamingSource MovedSource = Source;
	MovedSource.Center = Moved;
	Module->bInterestDirty = false;
	TestTrue(TEXT("Subcell relocation enters through the public source API"), Module->UpdateSource(Source.Id, MovedSource));
	TestTrue(TEXT("Subcell relocation requests an exact source acknowledgment"), Module->bInterestDirty);
	TestFalse(TEXT("Same planning cell cannot release a new spawn candidate early"), Module->IsSourceAdmittedAt(Source.Id, Moved, 11));
	Accepted.Sources.FindChecked(Source.Id).Source.Center = Moved;
	Module->CurrentInterest = MakeShared<const FVoxelInterestSet, ESPMode::ThreadSafe>(Accepted);
	TestTrue(TEXT("New complete source snapshot acknowledges the candidate"), Module->IsSourceAdmittedAt(Source.Id, Moved, 11));
	TestFalse(TEXT("World epoch invalidates source acknowledgment"), Module->IsSourceAdmittedAt(Source.Id, Moved, 10));
	Module->DesiredInterest = Module->CurrentInterest;
	AddExpectedError(TEXT("Voxel interest build stopped"), EAutomationExpectedErrorFlags::Contains, 1);
	Module->RecordInterestBuildFailure(TEXT("Oversized: interest failure fixture"), Module->SourceRevision,
		Module->InterestAttemptSerial + 1);
	TestTrue(TEXT("当前输入的确定性错误进入终态"), Module->HasInterestBuildFailed());
	TestFalse(TEXT("终态保留可展示的错误"), Module->GetInterestBuildError().IsEmpty());
	const uint64 FailedAttempt = Module->InterestAttemptSerial;
	Module->RefreshInterest(FPlatformTime::Seconds());
	Module->RefreshInterest(FPlatformTime::Seconds() + 1.0);
	TestEqual(TEXT("同一输入不再建立重试任务"), Module->InterestAttemptSerial, FailedAttempt);
	++Module->Epoch;
	TestFalse(TEXT("新世界不继承旧终态"), Module->HasInterestBuildFailed());
	TestTrue(TEXT("新世界不展示旧错误"), Module->GetInterestBuildError().IsEmpty());
	--Module->Epoch;
	MovedSource.Center += FIntVector(1, 0, 0);
	TestTrue(TEXT("终态后仍能接受新的 Source 输入"), Module->UpdateSource(Source.Id, MovedSource));
	TestFalse(TEXT("真实 Source 变化解除旧输入终态"), Module->HasInterestBuildFailed());
	TestTrue(TEXT("新输入不展示旧输入错误"), Module->GetInterestBuildError().IsEmpty());
	Module->Sources.Remove(Source.Id);
	TestFalse(TEXT("Removed source cannot acknowledge a candidate"), Module->IsSourceAdmittedAt(Source.Id, Moved, 11));

	// 经真实 Scheduler 分多次推进；只取得 PlanningPending 不能完成原 Source 的接纳任务。
	UVoxelModule* PlanningModule = NewObject<UVoxelModule>(Owner);
	PlanningModule->Epoch = 12;
	ON_SCOPE_EXIT
	{
		FString StopError;
		PlanningModule->StopWorld(true, StopError);
	};
	FString Error;
	UVoxelData* Definition = NewObject<UVoxelData>();
	Definition->BlockName = TEXT("test:interest_continuation");
	if (!TestTrue(TEXT("Native planning fixture builds its real registry"), PlanningModule->Registry.Build({Definition}, false, Error))) return false;
	const auto Shapes = MakeShared<FVoxelShapeRegistry, ESPMode::ThreadSafe>();
	Shapes->BuildDefaults();
	PlanningModule->Shapes = Shapes;
	const auto Control = FVoxelGenerationConsumerKey::Section(12, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> GeometryResources, FactResources;
	if (!TestTrue(TEXT("Native planning fixture creates both resource ledgers and the real scheduler"),
		FVoxelTerrainResources::Create({}, Control, GeometryResources, Error) &&
		FVoxelTerrainResources::Create({}, Control, FactResources, Error) &&
		FVoxelTaskScheduler::Create(GeometryResources.ToSharedRef(), FactResources.ToSharedRef(), Control, PlanningModule->Scheduler, Error))) return false;
	if (!PlanningModule->Scheduler->InitializeStatistics(12) || !PlanningModule->TerrainFrameContext.Configure({}, Error)) return false;
	PlanningModule->Scheduler->BindFrameContext(PlanningModule->TerrainFrameContext);
	PlanningModule->Generator = VoxelTest::MakeGenerator();
	PlanningModule->Runtime = MakeUnique<FVoxelWorldRuntime>(12, true, PlanningModule->GetRegistry().ToSharedRef(),
		PlanningModule->Generator.ToSharedRef(), FactResources.ToSharedRef());
	PlanningModule->Manifest.WorldId = FGuid::NewGuid();
	PlanningModule->Manifest.RegistryHash = PlanningModule->GetRegistry()->Hash;
	PlanningModule->Manifest.BaseSampleHash = 17;
	PlanningModule->Manifest.Settings.MinZ = -32;
	PlanningModule->Manifest.Settings.MaxZ = 64;
	PlanningModule->ViewSettings.Lod.MaximumSurfaceSpatialLevel = 2;
	PlanningModule->WorldState = EVoxelWorldState::Running;
	PlanningModule->InterestRuntime = MakeShared<FVoxelInterestRuntime, ESPMode::ThreadSafe>();
	PlanningModule->DesiredInterestRuntime = PlanningModule->InterestRuntime;
	PlanningModule->CurrentInterest = MakeShared<FVoxelInterestSet, ESPMode::ThreadSafe>();
	PlanningModule->DesiredInterest = PlanningModule->CurrentInterest;
	FVoxelStreamingSource LargeSource;
	LargeSource.Capabilities = EVoxelStreamingCapability::WorldVisual;
	LargeSource.bInheritWorldView = false;
	LargeSource.View.FineRadiusCells = 0;
	LargeSource.View.VoxelProxyRadiusCells = 0;
	LargeSource.View.SurfaceRadiusCells = 512;
	LargeSource.View.MacroRadiusCells = 0;
	LargeSource.View.InitialChunkSideCells = 16;
	const FGuid LargeId = PlanningModule->RegisterSource(PlanningModule, LargeSource);
	if (!TestTrue(TEXT("Native planning source is registered through the public API"), LargeId.IsValid())) return false;
	{
		FVoxelInterestRuntime Fixture;
		FVoxelInterestSet Partial;
		FVoxelInterestDelta PartialDelta;
		bool bPartialSnapshot = false;
		const auto& RegisteredSource = PlanningModule->Sources.FindChecked(LargeId).Value;
		Fixture.Update(MakeArrayView(&RegisteredSource, 1), PlanningModule->Manifest, PlanningModule->ViewSettings,
			Partial, PartialDelta, bPartialSnapshot);
		if (!TestTrue(TEXT("Fixture genuinely exceeds one bounded coarse planning quantum"),
			!bPartialSnapshot && Fixture.GetCoarsePlanningPending() && Fixture.GetPlanningError().IsEmpty())) return false;
	}
	PlanningModule->RefreshInterest(FPlatformTime::Seconds());
	if (!TestTrue(TEXT("Actual interest planning attempt is admitted"), PlanningModule->bInterestBuildPending)) return false;
	const uint64 OriginalAttempt = PlanningModule->InterestAttemptSerial;
	TestFalse(TEXT("An admitted task cannot acknowledge an incomplete source"), PlanningModule->IsSourceAdmittedAt(LargeId, {}, 12));
	uint64 Frame = 0;
	const double Deadline = FPlatformTime::Seconds() + 10.0;
	while (PlanningModule->bInterestBuildPending && FPlatformTime::Seconds() < Deadline)
	{
		PlanningModule->TerrainFrameContext.Begin(12, ++Frame, FPlatformTime::Seconds(), {});
		PlanningModule->Scheduler->Tick([](FVoxelTaskResult&&) {}, 8.0);
		PlanningModule->TerrainFrameContext.End(FPlatformTime::Seconds());
		FPlatformProcess::Sleep(0.001f);
	}
	TestFalse(TEXT("The original bounded attempt eventually finishes"), PlanningModule->bInterestBuildPending);
	TestEqual(TEXT("Coarse resumptions preserve the original attempt identity"), PlanningModule->InterestAttemptSerial, OriginalAttempt);
	TestTrue(TEXT("Only a complete native source snapshot releases the acceptance barrier"),
		PlanningModule->IsSourceAdmittedAt(LargeId, {}, 12));
	const auto& CompleteSource = PlanningModule->Sources.FindChecked(LargeId).Value;
	const auto Expected = FVoxelInterestManager().Compute(MakeArrayView(&CompleteSource, 1),
		PlanningModule->Manifest, PlanningModule->ViewSettings);
	TestEqual(TEXT("Complete topology reference has no planning failure"), Expected.PlanningError, FString());
	if (!TestTrue(TEXT("Complete topology reference builds actual Surface demand"),
		Expected.PlanningError.IsEmpty() && !Expected.Surface.IsEmpty())) return false;
	TestEqual(TEXT("The native multi-quantum task commits the complete expected Surface count"),
		PlanningModule->CurrentInterest->Surface.Num(), Expected.Surface.Num());
	for (const auto& Key : Expected.Surface)
		TestTrue(TEXT("Every expected Surface page survives the original task resumptions"), PlanningModule->CurrentInterest->Surface.Contains(Key));
	TestEqual(TEXT("Complete planning commits exactly once"), PlanningModule->InterestRevision, uint64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCriticalDependencyAdmissionTest, "WHFramework.Voxel.Streaming.CriticalDependencyAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelCriticalDependencyAdmissionTest::RunTest(const FString& InParameters)
{
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Center = FIntVector(0, 0, 242);
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual | EVoxelStreamingCapability::Readiness;
	Source.bInheritWorldView = false;
	Source.View.FineRadiusCells = 96;
	Source.View.FineLevel0RadiusCells = 32;
	Source.View.FineVerticalHalfExtentCells = 64;
	FVoxelWorldManifest Manifest;
	FVoxelViewSettings Settings;
	const FVoxelInterestSet Planned = FVoxelInterestManager().Compute(MakeArrayView(&Source, 1), Manifest, Settings);
	if (!TestTrue(TEXT("Readiness fixture has playable Fine ownership"), Planned.PlayableFineKeys.Num() > 0 && Planned.FinePagePlan.IsValid()))
	{
		return false;
	}
	bool bHasVerticalBoundary = false;
	for (const FIntVector& Key : Planned.PlayableFineKeys)
	{
		const FVoxelExactDemand& Required = Planned.Exact.FindChecked(Key);
		TestTrue(
			TEXT("Every playable Fine Section receives critical mesh and data admission"), Required.bMovementCriticalFine && Required.bMovementCriticalData);
		const auto* Owner = Planned.FinePagePlan->SectionOwners.Find(Key);
		TestTrue(TEXT("Every playable Fine Section donates to its Page"), Owner && Planned.CriticalFinePages.Contains(*Owner));
		bHasVerticalBoundary |= FMath::Abs(Key.Z * 16 + 8 - Source.Center.Z) > Source.View.FineLevel0RadiusCells;
		for (const FIntVector Direction :
			{FIntVector(1, 0, 0), FIntVector(-1, 0, 0), FIntVector(0, 1, 0), FIntVector(0, -1, 0), FIntVector(0, 0, 1), FIntVector(0, 0, -1)})
		{
			if (const auto* Neighbor = Planned.Exact.Find(Key + Direction))
			{
				TestTrue(TEXT("Playable Fine halo retains critical data donation"), Neighbor->bMovementCriticalData);
			}
		}
	}
	TestTrue(TEXT("Fixture covers the cylinder boundary beyond the movement sphere"), bHasVerticalBoundary);
	UVoxelModule* Module = NewObject<UVoxelModule>();
	Module->Runtime = MakeUnique<FVoxelWorldRuntime>(1024, false, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	FVoxelTaskScheduler Scheduler(true);
	FVoxelViewManager Manager(*Module, Scheduler, 11);
	FVoxelInterestSet Interest;
	const FVoxelTerrainSpatialKey Dependency{EVoxelTerrainRepresentation::VoxelProxy, FIntVector::ZeroValue, 1};
	Interest.CriticalDependencies.Add(Dependency);
	FVoxelExactDemand Demand;
	Demand.bMovementCriticalFine = true;
	Interest.Exact.Add(FIntVector::ZeroValue, Demand);
	Module->CurrentInterest = MakeShared<const FVoxelInterestSet, ESPMode::ThreadSafe>(Interest);
	TestFalse(TEXT("Critical demand alone is not presented Fine coverage"), Manager.IsFinePresented(FIntVector::ZeroValue));
	TestTrue(TEXT("Proxy predecessor is explicitly admitted"), Manager.IsCriticalDependency(Dependency));
	TestFalse(TEXT("Unrelated Proxy expansion does not inherit Critical"),
		Manager.IsCriticalDependency({EVoxelTerrainRepresentation::VoxelProxy, FIntVector(100, 0, 0), 1}));
	const FVoxelFinePageKey Page{FIntVector::ZeroValue, 1};
	const FIntVector Sibling(1, 0, 0);
	auto FinePlan = MakeShared<FVoxelFinePagePlan, ESPMode::ThreadSafe>();
	FinePlan->SectionOwners.Add(FIntVector::ZeroValue, Page);
	FinePlan->SectionOwners.Add(Sibling, Page);
	Interest.FinePagePlan = FinePlan;
	Interest.CriticalFinePages.Add(Page);
	Interest.Exact.Add(Sibling, FVoxelExactDemand());
	Module->CurrentInterest = MakeShared<const FVoxelInterestSet, ESPMode::ThreadSafe>(Interest);
	FVoxelViewAdmission SiblingAdmission;
	SiblingAdmission.Kind = EVoxelViewAdmissionKind::Fine;
	SiblingAdmission.FineKey = Sibling;
	TestTrue(TEXT("Every Section needed by a critical Fine Page remains admissible"), Manager.IsCriticalDependency(SiblingAdmission));
	FVoxelSection* Section = Module->Runtime->FindOrAllocate(FIntVector::ZeroValue, 1);
	Section->Status = EVoxelSectionStatus::DataReady;
	Manager.FineReady.Add(FIntVector::ZeroValue);
	TestFalse(TEXT("Ready mesh still needs the render receipt"), Manager.IsFinePresented(FIntVector::ZeroValue));
	Manager.PresentedFineSectionOwners.Add(FIntVector::ZeroValue, Page);
	TestFalse(TEXT("Derived layout ownership cannot impersonate the Native render receipt"), Manager.IsFinePresented(FIntVector::ZeroValue));
	Interest.CriticalDependencies.Reset();
	Interest.CriticalFinePages.Reset();
	Interest.Exact.Reset();
	Module->CurrentInterest = MakeShared<const FVoxelInterestSet, ESPMode::ThreadSafe>(Interest);
	TestFalse(TEXT("Donation is withdrawn with its demand"), Manager.IsCriticalDependency(Dependency));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNegativeCoverageContinuationTest, "WHFramework.Voxel.Rendering.NegativeCoverageContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelNegativeCoverageContinuationTest::RunTest(const FString& InParameters)
{
	FVoxelCoverageIndex Index;
	FString Error;
	for (int32 X : {-33, -2, -1, 0, 40})
	{
		auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
		Record->Geometry = {{EVoxelTerrainRepresentation::Fine, FIntVector(X, -1, 0), 0}, 1, -512, 512, 1, 1};
		Record->Geometry.Key.TryBounds(-512, 512, Record->Min, Record->Max);
		TestTrue(TEXT("Negative fixture is indexed"), Index.Upsert(Record, Error));
	}
	auto Upper = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
	Upper->Geometry = {{EVoxelTerrainRepresentation::Fine, FIntVector(-1, -1, 8), 0}, 1, -512, 512, 1, 1};
	Upper->Geometry.Key.TryBounds(-512, 512, Upper->Min, Upper->Max);
	TestTrue(TEXT("An independent Section in the same vertical column is indexed"), Index.Upsert(Upper, Error));
	FVoxelCoverageLocalSnapshot Sync;
	FVoxelCoverageLocalSnapshot Incremental;
	Index.Capture(FIntVector(-40, -20, -1), FIntVector(1, 1, 20), Sync);
	auto Steps = Index.CaptureSteps(FIntVector(-40, -20, -1), FIntVector(1, 1, 20), Incremental);
	int32 Operations = 0;
	while (!Steps.IsComplete() && Operations++ < 10000)
		Steps.Resume();
	TestTrue(TEXT("Inner traversal finishes under bounded resumptions"), Steps.IsComplete());
	TestTrue(TEXT("The query genuinely spans more than one microstep"), Operations > 1);
	TestEqual(TEXT("Negative query preserves coverage"), Incremental.Records.Num(), Sync.Records.Num());
	for (const auto& Record : Sync.Records)
		TestTrue(TEXT("Every synchronous record survives resumed capture"), Incremental.Records.Contains(Record));
	TestTrue(TEXT("Completed local read guards remain current"), Index.IsCurrent(Incremental));
	auto ChangedUpper = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>(*Upper);
	ChangedUpper->bReady = true;
	TestTrue(TEXT("The independent height changes readiness"), Index.Upsert(ChangedUpper, Error));
	TestTrue(TEXT("An unrelated height cannot invalidate a volume publication guard"), Index.IsCurrent(Incremental));
	if (!TestTrue(TEXT("Volume fixture contains a local input"), !Sync.Records.IsEmpty()))
	{
		return false;
	}
	auto Changed = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>(*Sync.Records[0]);
	Changed->bReady = true;
	Changed->Stamp.ContentRevision = 1;
	TestTrue(TEXT("A local readiness change is indexed"), Index.Upsert(Changed, Error));
	TestFalse(TEXT("A changed volume input still invalidates its publication guard"), Index.IsCurrent(Incremental));
	TestTrue(TEXT("The local input can return to its original values"), Index.Upsert(Sync.Records[0], Error));
	TestFalse(TEXT("An input change and restoration cannot revive a stale publication guard"), Index.IsCurrent(Incremental));
	FVoxelCoverageLocalSnapshot Full;
	auto FullSteps = Index.CaptureAllSteps(Full, false);
	while (!FullSteps.IsComplete())
	{
		FullSteps.Resume();
	}
	TestTrue(TEXT("The full index snapshot starts current"), Index.IsCurrent(Full));
	auto NewRoot = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
	NewRoot->Geometry = {{EVoxelTerrainRepresentation::Fine, FIntVector(1 << 20, 0, 0), 0}, 1, -512, 512, 1, 1};
	NewRoot->Geometry.Key.TryBounds(-512, 512, NewRoot->Min, NewRoot->Max);
	TestTrue(TEXT("A new root outside the captured root list is indexed"), Index.Upsert(NewRoot, Error));
	TestFalse(TEXT("Full index guards reject membership in a newly introduced root"), Index.IsCurrent(Full));
	const FVoxelCoverageRect Target{FIntPoint(-32, -16), FIntPoint(16, 16)};
	const TArray<FVoxelCoverageRect> Coverage{{FIntPoint(-32, -16), FIntPoint(0, 16)}, {FIntPoint(0, -16), FIntPoint(16, 16)}};
	bool bCovered = false;
	auto Query = VoxelCoverage::IsFullyCovered2DSteps(Target, Coverage, bCovered);
	while (!Query.IsComplete())
		Query.Resume();
	TestTrue(TEXT("Resumable subtraction covers negative and positive cells"), bCovered);
	const TArray<FVoxelCoverageRect> Gap{{FIntPoint(-32, -16), FIntPoint(-1, 16)}, {FIntPoint(0, -16), FIntPoint(16, 16)}};
	Query = VoxelCoverage::IsFullyCovered2DSteps(Target, Gap, bCovered);
	while (!Query.IsComplete())
		Query.Resume();
	TestFalse(TEXT("A one-cell hole is never accepted as covered"), bCovered);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainCostAndBudgetLivenessTest, "WHFramework.Voxel.Streaming.TerrainCostAndBudgetLiveness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelTerrainCostAndBudgetLivenessTest::RunTest(const FString& InParameters)
{
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	const FVoxelTerrainStageSource Stage{Source, 256, 64, 0};
	FVoxelTerrainStagePlan Plan;
	FString Error;
	for (const int32 Invalid : {16, 48, 256})
		TestFalse(TEXT("Proxy rejects invalid runtime sample caps"),
			FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy, MakeArrayView(&Stage, 1), 5, Invalid, -512, 512, 8.0, 1, 1, Plan, Error));
	FVoxelTerrainPageCostLimits Impossible;
	Impossible.Triangles = 1;
	TestFalse(TEXT("Unsplittable minimum legal page reports blocked cost"),
		FVoxelTerrainStagePlanner::Build(
			EVoxelTerrainRepresentation::VoxelProxy, MakeArrayView(&Stage, 1), 5, 32, -512, 512, 8.0, 1, 1, Plan, Error, nullptr, Impossible));
	FVoxelStreamingPerformanceBudget Settings;
	Settings.TerrainPublishMilliseconds = 0.1f;
	Settings.PublishOperationEstimateFloorMilliseconds = 0.01f;
	Settings.PublishOperationEstimateCeilingMilliseconds = 0.05f;
	TestTrue(TEXT("A small valid frame budget remains usable"), VoxelViewLod::ValidatePerformanceBudget(Settings, Error));
	FVoxelPublishBudget Budget;
	Budget.Begin(1.0, 0.1, 8);
	TestTrue(TEXT("A bounded microstep fits a small frame"), Budget.TryStart(EVoxelPublishPhase::Plan, 1.0, 0.01));
	Settings.PublishOperationEstimateFloorMilliseconds = 0.2f;
	TestFalse(TEXT("Floor larger than frame is rejected at runtime"), VoxelViewLod::ValidatePerformanceBudget(Settings, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCriticalReadyQueueLivenessTest, "WHFramework.Voxel.Streaming.CriticalReadyQueueLiveness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCriticalReadyQueueLivenessTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	const auto Generator = MakeShared<FVoxelGenerationPipeline, ESPMode::ThreadSafe>(Config, Cache);
	const auto Registry = VoxelTest::MakeRegistry();
	FVoxelWorldRuntime Runtime(11, false, Registry, Generator);
	FVoxelTaskScheduler Scheduler(true);
	FVoxelRegionStore Store;
	FVoxelWorldManifest Manifest;
	FVoxelEmergeManager Manager(Runtime, Scheduler, Generator, Config, Cache, Store, Manifest, Registry);
	FVoxelInterestSet Interest;
	for (int32 Index = 0; Index < 256; ++Index)
	{
		const FIntVector Key(Index, 0, 0);
		FVoxelExactDemand Demand;
		Demand.bWarmupData = true;
		Demand.bSimulation = true;
		Interest.Exact.Add(Key, Demand);
		Interest.ExactOrder.Add(Key);
		Interest.ExactAdmissionLanes[0].Add(Index);
		Runtime.FindOrAllocate(Key, 0)->Status = EVoxelSectionStatus::BaseReady;
	}
	const FIntVector Tail(255, 0, 0);
	Runtime.FindSection(Tail)->Status = EVoxelSectionStatus::DataReady;
	for (int32 Frame = 0; Frame < 8 && !Runtime.FindSection(Tail)->bSimulationWanted; ++Frame)
	{
		++Interest.PriorityRevision;
		Manager.Tick(Interest, 1, Frame, 1, 1000.0);
	}
	TestTrue(TEXT("Ready tail progresses behind blocked heads across continuous priority updates"), Runtime.FindSection(Tail)->bSimulationWanted);
	Manager.ReadyCriticalSections.Empty();
	Manager.QueuedCriticalSections.Reset();
	Manager.WakeCriticalSection(Tail);
	Manager.WakeCriticalSection(Tail);
	TestEqual(TEXT("Duplicate dependency wakeups share one ready entry"), Manager.QueuedCriticalSections.Num(), 1);
	const auto Resources = Scheduler.GetTerrainResources();
	const auto State = Resources->Snapshot();
	const auto OtherConsumer = FVoxelGenerationConsumerKey::Section(Runtime.Epoch(), FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkingBlocker;
	const auto BlockAdmission = Resources->TryAcquire(OtherConsumer, EVoxelTerrainResourcePool::Working,
		State.Limits.WorkingBytes - State.Used[int32(EVoxelTerrainResourcePool::Working)], WorkingBlocker);
	TestTrue(TEXT("External consumer fills original Working pool"), BlockAdmission == EVoxelTerrainResourceAdmission::Granted);
	if (WorkingBlocker)
	{
		FIntVector BeforeHead;
		TestTrue(TEXT("Original ready head is present"), Manager.ReadyCriticalSections.Peek(BeforeHead));
		const int32 BeforeLane0 = Manager.NextAdmissionIndices[0];
		const int32 BeforeLane1 = Manager.NextAdmissionIndices[1];
		const int32 BeforeAges = Manager.AdmissionAges.Num();
		const uint64 BeforeSequence = Manager.NextAdmissionSequence;
		Manager.Tick(Interest, 1, 10.0, 1, 1000.0);
		TestTrue(TEXT("Tick scratch reports recoverable original pool refusal"),
			Manager.GetLastResourceAdmission() == EVoxelTerrainResourceAdmission::CapacityBlocked);
		FIntVector AfterHead;
		TestTrue(TEXT("Refused scratch retains ready head"), Manager.ReadyCriticalSections.Peek(AfterHead));
		TestEqual(TEXT("Ready head identity is unchanged"), AfterHead, BeforeHead);
		TestEqual(TEXT("First lane cursor is unchanged"), Manager.NextAdmissionIndices[0], BeforeLane0);
		TestEqual(TEXT("Second lane cursor is unchanged"), Manager.NextAdmissionIndices[1], BeforeLane1);
		TestEqual(TEXT("Service age map is unchanged"), Manager.AdmissionAges.Num(), BeforeAges);
		TestEqual(TEXT("Refusal does not mint a service sequence"), Manager.NextAdmissionSequence, BeforeSequence);
		const uint64 BeforeWaitVersion = Manager.TickCapacityVersion;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OwnRefund;
		TestTrue(TEXT("Original control can hold an Input fixture lease"), Resources->TryAcquire(Manager.GetAdmissionConsumer(),
			EVoxelTerrainResourcePool::Input, 1, OwnRefund) == EVoxelTerrainResourceAdmission::Granted);
		OwnRefund.Reset();
		Manager.Tick(Interest, 1, 11.0, 1, 1000.0);
		TestEqual(TEXT("Own refund does not advance the original wait version"), Manager.TickCapacityVersion, BeforeWaitVersion);
		TestTrue(TEXT("Own refund leaves the original ready head pending"), Manager.ReadyCriticalSections.Peek(AfterHead));
		TestEqual(TEXT("Own refund retains head identity"), AfterHead, BeforeHead);
		WorkingBlocker.Reset();
		Manager.Tick(Interest, 1, 12.0, 1, 1000.0);
		TestTrue(TEXT("Different consumer release resumes the original Tick"),
			Manager.GetLastResourceAdmission() == EVoxelTerrainResourceAdmission::Granted);
		TestTrue(TEXT("Resumed Tick consumes the retained ready head"), Manager.ReadyCriticalSections.IsEmpty());
	}
	Manager.Reset();
	Manager.WakeCriticalSection(Tail);
	TestTrue(TEXT("A removed consumer cannot be awakened"), Manager.QueuedCriticalSections.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPlanConsumerWakeTest, "WHFramework.Voxel.Generation.PlanConsumerWake", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPlanConsumerWakeTest::RunTest(const FString& InParameters)
{
	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	FVoxelTaskScheduler Scheduler(true);
	FVoxelGenerationPlanCoordinator Coordinator(Scheduler, Config, Cache, 11);
	FVoxelGenerationPlanDependency Dependency;
	Dependency.Kind = EVoxelGenerationPlanKind::Hydrology;
	Dependency.HydrologyKey.Coordinate = FIntPoint::ZeroValue;
	const FGuid Owner = FGuid::NewGuid();
	const auto Critical = FVoxelGenerationConsumerKey::Section(11, Owner, 1, FIntVector::ZeroValue, 1);
	const auto Visible = FVoxelGenerationConsumerKey::Section(11, Owner, 1, FIntVector(1, 0, 0), 2);
	TSet<FVoxelGenerationConsumerKey> Woken;
	Coordinator.SetConsumerWakeCallback(
		[&Woken](const FVoxelGenerationConsumerKey& Consumer)
		{
			Woken.Add(Consumer);
		});
	Coordinator.Ensure(MakeArrayView(&Dependency, 1), EVoxelWorkClass::Critical, 0, 0.0, 1.0, Critical);
	Coordinator.Ensure(MakeArrayView(&Dependency, 1), EVoxelWorkClass::Visible, 3, 32.0, 0.0, Visible);
	TestEqual(TEXT("Consumers share one dependency attempt"), Scheduler.ActiveCount(), 1);
	Coordinator.ReleaseConsumer(Critical);
	TestEqual(TEXT("Critical reservation is returned after withdrawal"), Scheduler.GetDiagnostics().CriticalPending, 0);
	Scheduler.CancelMatching(
		[](EVoxelTaskKind Kind, const FVoxelTaskStamp&)
		{
			return Kind == EVoxelTaskKind::BuildGenerationPlan;
		});
	const double WakeDeadline = FPlatformTime::Seconds() + 2.0;
	while (!Woken.Contains(Visible) && FPlatformTime::Seconds() < WakeDeadline)
	{
		Scheduler.Tick(
			[](FVoxelTaskResult&&)
			{
			},
			8.0);
		Coordinator.AdvanceConsumerWakes(WakeDeadline, 64);
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("Matching cancellation wakes the remaining consumer"), Woken.Contains(Visible));
	TestFalse(TEXT("Released consumer is not awakened by the old callback"), Woken.Contains(Critical));
	Coordinator.ReleaseConsumer(Visible);
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelScopedCoverageContinuationTest, "WHFramework.Voxel.Rendering.ScopedCoverageContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelScopedCoverageContinuationTest::RunTest(const FString& InParameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler(true);
	FVoxelViewPublisher Publisher(*Module, Scheduler, 11);
	AActor* Actor = World->SpawnActor<AActor>();
	const FVoxelPublishGroupKey Key{0, FIntVector::ZeroValue, 0};
	const FGuid Group = Publisher.BeginGroup(Key);
	auto& Entry = Publisher.Entries.Add(Actor);
	Entry.GroupKey = Key;
	Entry.Source = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	Entry.Scale = Module->BlockSize();
	Entry.bHasDesiredExclusions = true;
	Entry.DesiredWorldExclusions.Add(FBox(FVector(-32, -32, -16), FVector(32, 32, 16)));
	const TArray<FBox> Scopes{FBox(FVector(-8, -8, -16), FVector(8, 8, 16))};
	bool bSuccess = false;
	auto Steps = Publisher.SetScopedCoverageSteps(Group, Actor, {}, nullptr, Scopes, bSuccess);
	int32 Operations = 0;
	while (!Steps.IsComplete() && Operations++ < 10000)
	{
		Steps.Resume();
	}
	TestTrue(TEXT("Scoped subtraction can pause inside an actor update"), Operations > 1 && Steps.IsComplete() && bSuccess);
	const auto& Boxes = Publisher.Entries.FindChecked(Actor).DesiredWorldExclusions;
	double Volume = 0.0;
	for (const FBox& Box : Boxes)
	{
		Volume += Box.GetVolume();
		TestFalse(TEXT("Removed local scope leaves no stale exclusion inside"), Box.IsInside(FVector::ZeroVector));
	}
	TestEqual(TEXT("Exclusions outside the local scope remain intact"), Volume, 64.0 * 64.0 * 32.0 - 16.0 * 16.0 * 32.0);
	Publisher.Reset();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPublishEstimateRecoveryTest, "WHFramework.Voxel.Rendering.PublishEstimateRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPublishEstimateRecoveryTest::RunTest(const FString& InParameters)
{
	UVoxelModule* Module = NewObject<UVoxelModule>();
	Module->ViewSettings.Budget.TerrainPublishMilliseconds = 0.1f;
	Module->ViewSettings.Budget.PublishOperationEstimateFloorMilliseconds = 0.01f;
	Module->ViewSettings.Budget.PublishOperationEstimateCeilingMilliseconds = 1.0;
	FVoxelTaskScheduler Scheduler(true);
	FVoxelViewPublisher Publisher(*Module, Scheduler, 11);
	const uint8 Phase = uint8(EVoxelPublishPhase::Plan);
	Publisher.PhaseOperationEstimateMilliseconds[Phase] = 8.0;
	FVoxelPublishBudget Budget;
	int32 Executed = 0;
	for (int32 Frame = 0; Frame < 64; ++Frame)
	{
		Budget.Begin(FPlatformTime::Seconds() - 1.0, 0.1, 8);
		Publisher.RunBudgetedPhase(Budget, EVoxelPublishPhase::Plan,
			[&Executed]
			{
				++Executed;
				return true;
			});
	}
	TestEqual(TEXT("An expired hard deadline never executes work"), Executed, 0);
	TestEqual(TEXT("Consecutive deferrals remain observable"), Publisher.PhaseConsecutiveDeferrals[Phase], uint64(64));
	Budget.Begin(FPlatformTime::Seconds(), 0.1, 8);
	TestTrue(TEXT("A fresh small budget recovers without forcing an oversized estimate"), Publisher.RunBudgetedPhase(Budget, EVoxelPublishPhase::Plan,
		[&Executed]
		{
			++Executed;
			return true;
		}));
	TestEqual(TEXT("Successful progress clears consecutive deferrals"), Publisher.PhaseConsecutiveDeferrals[Phase], uint64(0));
	TestTrue(TEXT("The estimate is bounded by a valid frame fraction"), Publisher.PhaseOperationEstimateMilliseconds[Phase] <=
		double(Module->ViewSettings.Budget.TerrainPublishMilliseconds) * 0.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTaskAdmissionFailureTest, "WHFramework.Voxel.Streaming.TaskAdmissionFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTaskAdmissionFailureTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(22, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> GeometryResources, FactResources;
	TUniquePtr<FVoxelTaskScheduler> Scheduler;
	FString Error;
	if (!TestTrue(TEXT("以实际账本建立 Scheduler"),
		FVoxelTerrainResources::Create({}, Consumer, GeometryResources, Error) &&
		FVoxelTerrainResources::Create({}, Consumer, FactResources, Error) &&
		FVoxelTaskScheduler::Create(GeometryResources.ToSharedRef(), FactResources.ToSharedRef(), Consumer, Scheduler, Error))) return false;
	if (!TestTrue(TEXT("初始化真实调度统计与工作选择状态"), Scheduler->InitializeStatistics(22))) return false;
	if (!TestTrue(TEXT("为拒绝结果预授予队列容量"), Scheduler->PrepareQueues(2))) return false;
	FVoxelTaskRequest Rejected;
	Rejected.Kind = EVoxelTaskKind::BuildInterest;
	Rejected.Stamp.WorldEpoch = 22;
	Rejected.Stamp.Token = 1;
	Rejected.Stamp.OwnerId = Consumer.OwnerId;
	Rejected.Stamp.OwnerIncarnation = 1;
	Rejected.Sequence = 1;
	Rejected.ReservedBytes = 1;
	EVoxelTaskBlockReason Blocked;
	if (!TestTrue(TEXT("开工前取得生产执行槽和错误缓冲"),
		Scheduler->ReserveNativeExecution(Rejected, Blocked) == EVoxelTerrainResourceAdmission::Granted)) return false;
	Rejected.Blocked = EVoxelTaskBlockReason::Oversized;
	Scheduler->QueueCanceled(MoveTemp(Rejected), EVoxelTaskBlockReason::Oversized);
	TestFalse(TEXT("永久接纳拒绝不是取消"), Scheduler->Canceled.Last().Result.bCanceled);
	TestFalse(TEXT("永久接纳拒绝保持失败状态"), Scheduler->Canceled.Last().Result.bSuccess);
	TestTrue(TEXT("错误正文通过原生预授额缓冲保留原因"),
		Scheduler->Canceled.Last().Result.Error.StartsWith(TEXT("CapacityUnachievable:")));
	TestTrue(TEXT("延后转移结果仍携带错误缓冲授额"), Scheduler->Canceled.Last().Result.InputResourceLease.IsValid());
	FVoxelTaskRequest Canceled;
	Canceled.Kind = EVoxelTaskKind::BuildInterest;
	Canceled.Stamp.WorldEpoch = 22;
	Canceled.Stamp.Token = 2;
	Canceled.Blocked = EVoxelTaskBlockReason::Oversized;
	Scheduler->QueueCanceled(MoveTemp(Canceled));
	TestTrue(TEXT("真实取消不因旧 BlockReason 被改成失败"), Scheduler->Canceled.Last().Result.bCanceled);
	TestTrue(TEXT("真实取消不产生虚假的终态正文"), Scheduler->Canceled.Last().Result.Error.IsEmpty());
	int32 Failures = 0, Cancellations = 0;
	Scheduler->Tick([&Failures, &Cancellations](FVoxelTaskResult&& Result)
	{
		if (Result.bCanceled) ++Cancellations;
		else if (!Result.bSuccess && Result.Error.StartsWith(TEXT("CapacityUnachievable:"))) ++Failures;
	}, 8.0);
	TestEqual(TEXT("终态结果回调恰好一次"), Failures, 1);
	TestEqual(TEXT("取消结果回调恰好一次"), Cancellations, 1);
	Scheduler->Tick([](FVoxelTaskResult&&) {}, 8.0);
	TestTrue(TEXT("结果没有留下重复回调"), Scheduler->Canceled.IsEmpty());
	Scheduler->StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelInterestPriorityAllocationTest, "WHFramework.Voxel.Streaming.PriorityAllocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelInterestPriorityAllocationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelInterestSet Previous;
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Capabilities = EVoxelStreamingCapability::Data;
	Source.ExactRadius = 64;
	Source.Normalize();
	auto& Contribution = Previous.Sources.FindOrAdd(Source.Id);
	Contribution.Source = Source;
	for (int32 Index = 0; Index < 256; ++Index)
	{
		const FIntVector Key(Index % 16, Index / 16, 0);
		Contribution.DataSections.Add(Key);
		auto& Demand = Previous.Exact.FindOrAdd(Key);
		Demand.bData = true;
		Demand.Priority = Source.GetSchedulingPriority();
	}
	const uint64 PreviousOwned = Previous.GetOwnedAllocatedBytes();
	const uint64 PreviousFinalization = Previous.GetFinalizationAllocatedBytes();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(21, FGuid::NewGuid(), 1, {}, 1);
	FString Error;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	if (!TestTrue(TEXT("建立实际资源账本"), FVoxelTerrainResources::Create({}, Consumer, Resources, Error))) return false;
	FVoxelInterestManager Manager;
	Manager.BindResources(Resources.ToSharedRef(), Consumer);
	{
		FVoxelInterestSet Updated;
		if (!TestTrue(TEXT("重排前复制并授予增长容量"), Manager.CopyForPriority(Previous, Updated, Error)))
		{
			AddError(Error);
			return false;
		}
		TestTrue(TEXT("副本和重排分别持有真实租约"), Updated.CopyLease.IsValid() && Updated.FinalizationLease.IsValid());
		Source.Priority = 7;
		Source.SchedulingPriority = 7;
		Updated.Sources.FindChecked(Source.Id).Source = Source;
		for (auto& Pair : Updated.Exact) Pair.Value.bMovementCriticalData = Pair.Key.X < 8;
		if (!TestTrue(TEXT("通过生产重排路径结算新增 lane 容量"), FVoxelInterestManager::RefreshPriorities(
			MakeArrayView(&Source, 1), 32, Updated, Error, Resources, Consumer)))
		{
			AddError(Error);
			return false;
		}
		const uint64 Finalization = Updated.GetFinalizationAllocatedBytes();
		TestTrue(TEXT("夹具实际增长重排容器"), Finalization > PreviousFinalization);
		TestTrue(TEXT("增长后的真实分配全部有授额"), Updated.GetOwnedAllocatedBytes() <= Updated.GetResourceAccountedBytes());
		TestEqual(TEXT("重排授额按真实容量结算"), Updated.FinalizationLease->Bytes(), FMath::Max<uint64>(1, Finalization));
		TestEqual(TEXT("副本只保留非重排容器授额"), Updated.CopyLease->Bytes(),
			FMath::Max<uint64>(1, Updated.GetOwnedAllocatedBytes() - Finalization));
		TestEqual(TEXT("优先级更新成功"), Updated.Exact.FindChecked(FIntVector::ZeroValue).Priority, 7);
		TestEqual(TEXT("关键 lane 使用真实输入分流"), Updated.ExactAdmissionLanes[0].Num(), 128);
		TestEqual(TEXT("普通 lane 保留其余输入"), Updated.ExactAdmissionLanes[1].Num(), 128);
		TestEqual(TEXT("旧快照不被重排修改"), Previous.Exact.FindChecked(FIntVector::ZeroValue).Priority,
			Contribution.Source.GetSchedulingPriority());
	}
	TestEqual(TEXT("完成副本退休后释放所有临时租约"), Resources->Snapshot().LeaseCount, 0);

	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	const auto OtherConsumer = FVoxelGenerationConsumerKey::Section(21, FGuid::NewGuid(), 1, {}, 1);
	if (!TestTrue(TEXT("建立可释放的外部占用"), Resources->Acquire(OtherConsumer, EVoxelTerrainResourcePool::Input, 1, Blocker))) return false;
	const auto BeforeBlock = Resources->Snapshot();
	const uint64 FreeInput = BeforeBlock.Limits.InputBytes - BeforeBlock.Used[int32(EVoxelTerrainResourcePool::Input)];
	if (!TestTrue(TEXT("夹具保留可复制但不足以重排的空间"), FreeInput > PreviousOwned + 1024)) return false;
	if (!TestTrue(TEXT("外部占用约束实际 Input 池"), Blocker->Resize(Blocker->Bytes() + FreeInput - PreviousOwned - 1024))) return false;
	const int32 LeasesBeforeFailure = Resources->Snapshot().LeaseCount;
	{
		FVoxelInterestSet Failed;
		TestFalse(TEXT("容量不足不执行无授额的重排"), Manager.CopyForPriority(Previous, Failed, Error));
		TestTrue(TEXT("外部占用被区分为可恢复阻塞"), Error.StartsWith(TEXT("CapacityBlocked:")));
		TestTrue(TEXT("失败点确实在已取得副本之后"), Failed.CopyLease.IsValid());
	}
	TestEqual(TEXT("失败副本退出后不持有自身等待的租约"), Resources->Snapshot().LeaseCount, LeasesBeforeFailure);
	const uint64 BeforeRelease = Resources->ReleaseVersionExcludingOwner(Consumer);
	Blocker.Reset();
	TestTrue(TEXT("外部释放能够唤醒原消费者"), Resources->ReleaseVersionExcludingOwner(Consumer) > BeforeRelease);
	FVoxelInterestSet Retried;
	if (!TestTrue(TEXT("外部释放后同一输入可恢复"), Manager.CopyForPriority(Previous, Retried, Error))) return false;
	Retried.Sources.FindChecked(Source.Id).Source = Source;
	TestTrue(TEXT("恢复后仍通过完整生产结算"), FVoxelInterestManager::RefreshPriorities(
		MakeArrayView(&Source, 1), 32, Retried, Error, Resources, Consumer));
	TestTrue(TEXT("恢复后所有输出分配有授额"), Retried.GetOwnedAllocatedBytes() <= Retried.GetResourceAccountedBytes());
	return true;
}

#endif
