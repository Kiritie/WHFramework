#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Rendering/VoxelTerrainStagePlanner.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelProxyPageBuild.h"
#include "Voxel/Rendering/VoxelHeightfieldPageBuild.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include "Voxel/Geometry/DWVoxelBoundaryTransition.h"
#include "Voxel/VoxelModule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelActualSamplingLodTest,
	"WHFramework.Voxel.Rendering.ActualSamplingLOD",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelActualSamplingLodTest::RunTest(const FString& InParameters)
{
	FString Error;
	const FVoxelTerrainSpatialKey Spatial{EVoxelTerrainRepresentation::VoxelProxy, FIntVector(-1, 0, 0), 3};
	for (const int32 Step : {2, 4, 8, 16, 32, 64})
	{
		const FVoxelTerrainGeometry Geometry{Spatial, Step, -128, 256, 17, 23};
		TestTrue(TEXT("One spatial key admits multiple explicit legal sample steps"), Geometry.Validate(Error));
		TestEqual(TEXT("Actual grid and physical step cover the entire spatial node"),
			(Geometry.Key.SideCells() / Geometry.SampleStepCells) * Geometry.SampleStepCells, Geometry.Key.SideCells());
	}
	FVoxelTerrainGeometry Fine{{EVoxelTerrainRepresentation::Fine, FIntVector::ZeroValue, 0}, 1, -128, 256, 17, 23};
	TestTrue(TEXT("Fine retains exact one-cell sampling"), Fine.Validate(Error));
	Fine.SampleStepCells = 2;
	TestFalse(TEXT("Fine cannot silently adopt coarse sampling"), Fine.Validate(Error));
	FVoxelTerrainGeometry Invalid{Spatial, 1, -128, 256, 17, 23};
	TestFalse(TEXT("Proxy rejects Fine sampling"), Invalid.Validate(Error));
	Invalid.SampleStepCells = 3;
	TestFalse(TEXT("Proxy rejects non-power-of-two sampling"), Invalid.Validate(Error));
	Invalid.SampleStepCells = 256;
	TestFalse(TEXT("Sampling must divide actual spatial coverage"), Invalid.Validate(Error));

	int32 Step = 0;
	bool bFloor = false;
	TestTrue(TEXT("SSE immediately before the sixteen-cell threshold resolves"),
		VoxelStreamingGeometry::ResolveSampleStep(599, 90.0, 600, 8.0, 128, Step, bFloor, Error, 2));
	TestEqual(TEXT("Sixteen-cell sampling is ineligible before its actual pixel threshold"), Step, 8);
	TestTrue(TEXT("SSE immediately after the sixteen-cell threshold resolves"),
		VoxelStreamingGeometry::ResolveSampleStep(601, 90.0, 600, 8.0, 128, Step, bFloor, Error, 2));
	TestEqual(TEXT("Sixteen-cell sampling becomes eligible independently of spatial level"), Step, 16);
	TestTrue(TEXT("Sampling can exceed the removed legacy sixteen-cell ceiling"),
		VoxelStreamingGeometry::ResolveSampleStep(5000, 90.0, 600, 8.0, 128, Step, bFloor, Error, 2));
	TestEqual(TEXT("Distant sampling is capped by actual geometry bounds"), Step, 128);
	TestEqual(TEXT("Texture period remains independent of spatial level"), VoxelViewLod::TexturePeriodCells(32, 8.0), 8.0);

	TArray<FVoxelTerrainBuildShard> Shards;
	const FVoxelTerrainGeometry Finer{Spatial, 2, -128, 256, 17, 23};
	const FVoxelTerrainGeometry Coarser{Spatial, 4, -128, 256, 17, 23};
	TestTrue(TEXT("Finer actual geometry splits into bounded execution shards"),
		VoxelStreamingGeometry::BuildShards(Finer, 32, 64, Shards, Error));
	TestEqual(TEXT("Sixty-four samples per axis require eight bounded shards"), Shards.Num(), 8);
	TestTrue(TEXT("Same spatial coverage uses fewer shards with coarser actual sampling"),
		VoxelStreamingGeometry::BuildShards(Coarser, 32, 64, Shards, Error));
	TestEqual(TEXT("Thirty-two samples per axis fit one bounded shard"), Shards.Num(), 1);
	TestFalse(TEXT("Invalid geometry is rejected before replacing the shard plan"),
		VoxelStreamingGeometry::BuildShards(Invalid, 32, 64, Shards, Error));
	TestEqual(TEXT("Rejected sampling retains the previous shard plan"), Shards.Num(), 1);
	TestFalse(TEXT("Non-power-of-two execution limits cannot create incompatible edge windows"),
		VoxelStreamingGeometry::BuildShards(Finer, 15, 512, Shards, Error));
	TestEqual(TEXT("Rejected execution limits retain the previous shard plan"), Shards.Num(), 1);

	FVoxelProxyPageBuild ProxyBuild;
	TestTrue(TEXT("Production Proxy page admits explicit finer geometry"), ProxyBuild.Initialize(Finer, Error));
	const uint64 ProxyRetainedBytes = ProxyBuild.MaximumRetainedBytes();
	TestFalse(TEXT("Invalid Proxy page initialization is rejected"), ProxyBuild.Initialize(Invalid, Error));
	TestEqual(TEXT("Rejected Proxy initialization preserves the old admitted geometry"), ProxyBuild.MaximumRetainedBytes(), ProxyRetainedBytes);
	FVoxelHeightfieldPageBuild HeightfieldBuild;
	const FVoxelTerrainGeometry Surface{{EVoxelTerrainRepresentation::Surface, FIntVector::ZeroValue, 1}, 1, -128, 256, 17, 23};
	TestTrue(TEXT("Production heightfield page admits explicit column geometry"), HeightfieldBuild.Initialize(Surface, Error));
	const uint64 HeightfieldRetainedBytes = HeightfieldBuild.MaximumRetainedBytes();
	TestFalse(TEXT("Heightfield rejects a volume representation before replacing its state"), HeightfieldBuild.Initialize(Finer, Error));
	TestEqual(TEXT("Rejected heightfield initialization preserves the admitted geometry"), HeightfieldBuild.MaximumRetainedBytes(), HeightfieldRetainedBytes);

	FIntVector SamplingMin;
	FIntVector SamplingMax;
	const FVoxelTerrainGeometry Edge{{EVoxelTerrainRepresentation::VoxelProxy, FIntVector(MAX_int32 / 32 - 1, 0, 0), 1}, 2, -128, 256, 17, 23};
	TestTrue(TEXT("Representable geometry has valid unpadded sampling bounds"),
		VoxelStreamingGeometry::TrySamplingBounds(Edge, FIntVector::ZeroValue, FIntVector::ZeroValue, SamplingMin, SamplingMax, Error));
	const FIntVector RetainedMin = SamplingMin;
	const FIntVector RetainedMax = SamplingMax;
	TestFalse(TEXT("Sampling padding cannot overflow the integer coordinate range"),
		VoxelStreamingGeometry::TrySamplingBounds(Edge, FIntVector::ZeroValue, FIntVector(32), SamplingMin, SamplingMax, Error));
	TestFalse(TEXT("Negative sampling padding is rejected"),
		VoxelStreamingGeometry::TrySamplingBounds(Edge, FIntVector(-1, 0, 0), FIntVector::ZeroValue, SamplingMin, SamplingMax, Error));
	TestEqual(TEXT("Rejected sampling bounds preserve the previous minimum"), SamplingMin, RetainedMin);
	TestEqual(TEXT("Rejected sampling bounds preserve the previous maximum"), SamplingMax, RetainedMax);

	const auto Config = VoxelTest::MakeGenerationConfig();
	const auto Cache = MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
	const FVoxelVoxelProxyBuilder Builder(Config, Cache);
	FVoxelVoxelProxyData RetainedData;
	RetainedData.Geometry = Coarser;
	RetainedData.Revision = 73;
	RetainedData.Cells.Add(FVoxelBlockState(1, 0));
	const TArray<FVoxelTerrainBuildShard> InvalidWindows{
		{FIntVector::ZeroValue, FIntVector(16, 8, 16)},
		{FIntVector(1, 0, 0), FIntVector(16)},
		{FIntVector::ZeroValue, FIntVector(-1)},
		{FIntVector(128, 0, 0), FIntVector(16)},
		{FIntVector::ZeroValue, FIntVector(129)}};
	for (const FVoxelTerrainBuildShard& Window : InvalidWindows)
	{
		TestFalse(TEXT("Production Proxy builder rejects invalid windows before sampling"),
			Builder.BuildNatural({Spatial.Coordinate, Spatial.Level}, RetainedData, Error, nullptr, Finer, &Window));
		TestTrue(TEXT("Rejected Proxy sampling preserves the original geometry"), RetainedData.Geometry == Coarser);
		TestEqual(TEXT("Rejected Proxy sampling preserves the original revision"), RetainedData.Revision, uint64(73));
		TestEqual(TEXT("Rejected Proxy sampling preserves the original payload"), RetainedData.Cells.Num(), 1);
	}

	FVoxelTerrainStageSource Strict;
	Strict.Source.Center = FIntVector::ZeroValue;
	Strict.Source.VerticalFovDegrees = 90.0f;
	Strict.Source.ViewportHeightPixels = 600;
	Strict.OuterRadiusCells = 128;
	Strict.VerticalHalfExtentCells = 32;
	FVoxelTerrainStageSource Relaxed = Strict;
	Relaxed.Source.ViewportHeightPixels = 100;
	FVoxelTerrainStagePlan StrictPlan;
	FVoxelTerrainStagePlan CombinedPlan;
	const TArray<FVoxelTerrainStageSource> Sources{Relaxed, Strict};
	if (!TestTrue(TEXT("Production planner resolves the strict local source"),
		FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy, MakeArrayView(&Strict, 1),
			3, 64, -32, 64, 8.0, 17, 23, StrictPlan, Error)) ||
		!TestTrue(TEXT("Production planner resolves overlapping source projections"),
		FVoxelTerrainStagePlanner::Build(EVoxelTerrainRepresentation::VoxelProxy, Sources,
			3, 64, -32, 64, 8.0, 17, 23, CombinedPlan, Error))) return false;
	TestEqual(TEXT("Relaxed source cannot coarsen the strict spatial plan"), CombinedPlan.Pages.Num(), StrictPlan.Pages.Num());
	for (const auto& Pair : StrictPlan.Pages)
	{
		const FVoxelTerrainGeometry* Combined = CombinedPlan.Pages.Find(Pair.Key);
		TestTrue(TEXT("Overlapping sources select the strictest actual local sampling"), Combined && *Combined == Pair.Value);
	}
	TestFalse(TEXT("Near Proxy quality floor is explicitly retained"), CombinedPlan.ResolutionFloorPages.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPublisherStageAndIndexTest,
	"WHFramework.Voxel.Rendering.PublisherStageAndIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPublisherStageAndIndexTest::RunTest(const FString& InParameters)
{
	FString BuildError;
	(void)InParameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler(true);
	Scheduler.SetObservedTerrainStage(3);
	FVoxelViewPublisher Publisher(*Module, Scheduler, 1);
	const TArray<FVector> Observers{FVector::ZeroVector};
	Publisher.SetObservers(Observers);
	const auto Publication = MakeShared<FVoxelViewPublisher::FPublication>();
	Publication->Serial = 1;
	Publication->Updates.SetNum(2);
	Publication->PreparedMeshes.SetNum(2);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		auto& Entry = Publisher.Entries.Add(Actor);
		Entry.GroupKey = {1, FIntPoint(Index == 0 ? 100 : 0, 0), 1};
		Entry.Source = MakeShared<const FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
		Entry.Bounds = FBox(FVector::ZeroVector, FVector(32));
		Entry.Location = FVector(Index == 0 ? 10000 : 0, 0, 0);
		auto& Update = Publication->Updates[Index];
		Update.Actor = Actor;
		Update.Source = Entry.Source;
		Update.Location = Entry.Location;
		Update.Bounds = Entry.Bounds;
		Update.TerrainStage = Index == 0 ? 1 : 0;
		Publication->Callbacks.Add(Entry.GroupKey, {});
	}
	Publisher.BuildGroups(Publication);
	for (int32 Step = 0; Step < 16 && !Publication->bCoveragePrepared; ++Step) Publisher.AdmitCoveragePreparation(Publication);
	TestTrue(TEXT("Uncovered initial comparison completes synchronously"), Publication->bCoveragePrepared);
	// 覆盖精化重排 owner，先前批次中的 Source 不能留在旧物理索引。
	Publisher.BuildGroups(Publication, true);
	Publisher.SortGroups(Publication);
	Publication->bGroupsPrepared = true;
	for (int32 Step = 0; Step < 16 && !Publication->bBuildOrderInitialized; ++Step) Publisher.AdmitBuilds(Publication);
	TestTrue(TEXT("Build admission initialization completes within bounded work"), Publication->bBuildOrderInitialized);
	for (int32 Index = 0; Index < Publication->Updates.Num(); ++Index)
		TestTrue(TEXT("Every prepared direct mesh still belongs to its final update"),
			Publication->PreparedMeshes[Index] == Publication->Updates[Index].Source);
	Scheduler.SetObservedTerrainStage(0);
	for (const auto& Update : Publication->Updates)
	{
		const auto& Group = Publication->Groups[Update.GroupIndex];
		if (Update.TerrainStage != 0)
		{
			TestTrue(TEXT("Independent distant mesh is eligible at observed Stage zero"), Publisher.CanPublishGroup(Group));
			continue;
		}
		TestEqual(TEXT("A Proxy owner containing a Fine seam keeps actual Stage zero"), Publisher.GroupStage(Group), 0);
		TestTrue(TEXT("Fine-dependent Proxy publication passes the Fine gate"), Publisher.CanPublishGroup(Group));
	}
	const FVoxelPublishGroupKey FineOwner{0, FIntPoint::ZeroValue, 0};
	const FVoxelPublishGroupKey SurfaceOwner{2, FIntPoint::ZeroValue, 0};
	const auto Older = MakeShared<FVoxelViewPublisher::FPublication>();
	Older->Serial = 2;
	Older->bGroupsPrepared = true;
	Older->Groups.SetNum(3);
	Older->GroupOrder = {0, 1, 2};
	Older->Groups[0].Keys = {FineOwner};
	Older->Groups[0].Bounds = FBox(FVector::ZeroVector, FVector(16));
	Older->Groups[1].Keys = {SurfaceOwner};
	Older->Groups[1].Bounds = FBox(FVector(32, 0, 0), FVector(64, 16, 16));
	Older->Groups[2].Keys = {{2, FIntPoint(10, 0), 0}};
	Older->Groups[2].Bounds = FBox(FVector(1000, 0, 0), FVector(1100, 16, 16));
	const auto SecondGeneration = MakeShared<FVoxelViewPublisher::FPublication>();
	SecondGeneration->Serial = 3;
	SecondGeneration->bGroupsPrepared = true;
	SecondGeneration->Groups.SetNum(1);
	SecondGeneration->GroupOrder = {0};
	SecondGeneration->Groups[0].Keys = {FineOwner};
	SecondGeneration->Groups[0].Bounds = Older->Groups[0].Bounds;
	const auto Demand = MakeShared<FVoxelViewPublisher::FPublication>();
	Demand->Serial = 4;
	Demand->Groups.SetNum(1);
	Demand->GroupOrder = {0};
	Demand->Groups[0].Keys = {FineOwner, {0, FIntPoint(2, 0), 0}};
	Demand->Groups[0].Bounds = FBox(FVector::ZeroVector, FVector(64, 16, 16));
	const auto Future = MakeShared<FVoxelViewPublisher::FPublication>();
	Future->Serial = 5;
	Future->bGroupsPrepared = true;
	Future->Groups.SetNum(1);
	Future->GroupOrder = {0};
	Future->Groups[0].Keys = {{2, FIntPoint(20, 0), 0}};
	Future->Groups[0].Bounds = Older->Groups[1].Bounds;
	Publisher.Publications = {Older, SecondGeneration, Demand, Future};
	for (const auto& QueuedPublication : Publisher.Publications)
		for (const auto& Group : QueuedPublication->Groups)
			for (const auto& Key : Group.Keys) Publisher.SetOwnerActive(Key, QueuedPublication->Serial, true);
	TMap<FVoxelPublishGroupKey, FVoxelViewPublisher::FGroupPriority> DeferredPriorities;
	Publisher.DeferSaturatedGroups(Demand, DeferredPriorities);
	TestTrue(TEXT("Third Fine generation moves back to the pending area"), Demand->Groups.IsEmpty());
	Publisher.UpdateGroupPriorities(MoveTemp(DeferredPriorities));
	TestTrue(TEXT("Deferred Fine donates Stage zero to its spatial Surface predecessor"),
		Publisher.CanPublishGroup(Older->Groups[1]));
	TestTrue(TEXT("Independent Surface remains eligible without global stage gating"),
		Publisher.CanPublishGroup(Older->Groups[2]));
	TestTrue(TEXT("Newer publication eligibility is checked separately from its local dependency"),
		Publisher.CanPublishGroup(Future->Groups[0]));
	Publisher.Reset();
	bool bLogicalCommitted = false;
	Publisher.SetCommitCallbacks({}, [&bLogicalCommitted](TConstArrayView<FVoxelPublishGroupKey>)
	{
		bLogicalCommitted = true;
	});
	const FGuid LogicalGroup = Publisher.BeginGroup({1, FIntPoint::ZeroValue, 1});
	TestTrue(TEXT("Empty Proxy seam records its actual Fine dependency stage"), Publisher.SetTerrainStage(LogicalGroup, 0));
	Publisher.CommitGroup(LogicalGroup);
	Publisher.Tick();
	TestTrue(TEXT("Logical empty seam commits through the Fine gate without an Actor or mesh"), bLogicalCommitted);
	Scheduler.StopAndJoin();
	Publisher.Reset();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelEmptyTransitionConvergenceTest,
	"WHFramework.Voxel.Rendering.EmptyTransitionConvergence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEmptyTransitionConvergenceTest::RunTest(const FString& InParameters)
{
	FString BuildError;
	(void)InParameters;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler(true);
	Scheduler.SetObservedTerrainStage(3);
	FVoxelViewManager Manager(*Module, Scheduler, 7);
	const FVoxelViewKey Node{FIntVector::ZeroValue, 1};
	const FVoxelPublishGroupKey Owner{1, FIntPoint::ZeroValue, 1};
	auto& Interest = const_cast<FVoxelInterestSet&>(Module->GetCurrentInterest());
	Interest.TerrainPlan.Roots = {Node};
	Interest.TerrainPlan.Leaves = {Node};
	Interest.TerrainPlan.Required = {Node};
	Manager.GetInputRoot().ProxyWanted.Add(Node);
	Manager.VoxelProxyReady.Add(Node);
	Manager.CommitTerrainOwnership(Owner, MakeArrayView(&Node, 1));
	auto Data = MakeShared<FVoxelVoxelProxyData, ESPMode::ThreadSafe>();
	Data->Key = Node;
	Data->Geometry = {{EVoxelTerrainRepresentation::VoxelProxy, Node.Coordinate, Node.Level}, 2, -128, 256, 17, 23};
	Interest.Geometry.Add(Data->Geometry.Key, Data->Geometry);
	Data->Revision = 1;
	Manager.VoxelProxyData.Add(Node, Data);
	Manager.RebuildVolumeTransitions({Node});
	const double Deadline = FPlatformTime::Seconds() + 5.0;
	while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	Manager.RebuildVolumeTransitions({Node});
	const uint64 Signature = Manager.DesiredVolumeSignatures.FindRef(Node);
	Manager.PreparedVolumeSignatures.Add(Node, Signature);
	Manager.PreparedVolumeMeshes.Add(Node, MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>());
	Manager.ResolveTransitionVisibility();
	Manager.Publisher->Tick();
	TestEqual(TEXT("Empty seam result commits its signature without an Actor"), Manager.VolumeTransitionSignatures.FindRef(Node), Signature);
	TestFalse(TEXT("Empty seam preparation is released after its logical commit"), Manager.PreparedVolumeSignatures.Contains(Node));
	Manager.bCoverageDirty = false;
	Manager.bCoverageDirtyFull = false;
	Manager.ResolveTransitionVisibility();
	Manager.Publisher->Tick();
	TestFalse(TEXT("Stable empty seam no longer dirties coverage repeatedly"), Manager.bCoverageDirty);
	Scheduler.StopAndJoin();
	Manager.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineProxyDependenciesTest,
	"WHFramework.Voxel.Rendering.FineProxyDependencies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineProxyDependenciesTest::RunTest(const FString& InParameters)
{
	FString BuildError;
	(void)InParameters;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler(true);
	Scheduler.SetObservedTerrainStage(0);
	FVoxelViewManager Manager(*Module, Scheduler, 11);
	const FVoxelViewKey Dependency{FIntVector::ZeroValue, 1};
	const FVoxelViewKey Distant{FIntVector(10, 0, 0), 1};
	const FVoxelViewKey Root{FIntVector::ZeroValue, 5};
	auto& Interest = const_cast<FVoxelInterestSet&>(Module->GetCurrentInterest());
	Interest.TerrainPlan.Roots = {Root};
	Interest.TerrainPlan.Leaves = Interest.TerrainPlan.Required = Interest.TerrainPlan.Roots;
	for (uint8 Level = Root.Level; Level > 0; --Level)
	{
		const FVoxelViewKey Parent{FIntVector::ZeroValue, Level};
		Interest.TerrainPlan.Leaves.Remove(Parent);
		Interest.TerrainPlan.FineDependencies.Add(Parent);
		TArray<FVoxelViewKey> Children;
		Parent.GetChildren(Children);
		for (const FVoxelViewKey& Child : Children)
		{
			Interest.TerrainPlan.Leaves.Add(Child);
			Interest.TerrainPlan.Required.Add(Child);
			Interest.TerrainPlan.FineDependencies.Add(Child);
		}
	}
	TestTrue(TEXT("Dependency fixture has a complete spatial partition"), Interest.TerrainPlan.Validate(BuildError));
	TestTrue(TEXT("Spatial dependency fixture retains the Fine ancestor"),
		Interest.TerrainPlan.FineDependencies.Contains(Dependency));
	TestTrue(TEXT("Spatial dependency fixture includes the handoff sibling"),
		Interest.TerrainPlan.FineDependencies.Contains({FIntVector(1, 0, 0), 1}));
	FVoxelViewAdmission Admission;
	Admission.Kind = EVoxelViewAdmissionKind::VoxelProxy;
	Admission.ProxyKey = Dependency;
	TestEqual(TEXT("Fine dependency is admitted in the Fine stage"), Manager.AdmissionTerrainStage(Admission), 0);
	Admission.ProxyKey = Distant;
	TestEqual(TEXT("Independent Proxy retains its coarse admission stage"), Manager.AdmissionTerrainStage(Admission), 1);
	TestTrue(TEXT("Indirect coarse seam needed by Fine is interactive without a direct Fine patch"),
		Manager.VolumeTransitionWorkClass(Dependency) == EVoxelWorkClass::Interactive);
	TestTrue(TEXT("Independent coarse seam remains boundary work"),
		Manager.VolumeTransitionWorkClass(Distant) == EVoxelWorkClass::Boundary);
	TAtomic<int32> Executed[2];
	Executed[0].Store(0);
	Executed[1].Store(0);
	int32 Applied[2] = {};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::BuildVoxelProxy;
		Request.Stamp.ViewKey = Index == 0 ? Dependency : Distant;
		Request.Stamp.Token = Index;
		Request.TerrainStage = Manager.VoxelProxyTerrainStage(Request.Stamp.ViewKey);
		Request.WorkClass = Request.TerrainStage == 0 ? EVoxelWorkClass::Interactive : EVoxelWorkClass::Boundary;
		Request.ReservedBytes = 1024;
		Request.Execute = [Index, &Executed](const TAtomic<bool>& Cancel)
		{
			++Executed[Index];
			FVoxelTaskResult Result;
			Result.bSuccess = !Cancel.Load();
			return Result;
		};
		TestTrue(TEXT("Dependency fixture enters scheduler"), Scheduler.Enqueue(MoveTemp(Request)));
	}
	const auto Apply = [&Applied](FVoxelTaskResult&& Result)
	{
		if (Result.bSuccess && Result.Stamp.Token < 2) ++Applied[Result.Stamp.Token];
	};
	const double FineDeadline = FPlatformTime::Seconds() + 5.0;
	while ((Applied[0] == 0 || Applied[1] == 0) && FPlatformTime::Seconds() < FineDeadline)
	{
		Scheduler.Tick(Apply, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestEqual(TEXT("Fine-dependent Proxy builds and applies through Stage zero"), Applied[0], 1);
	TestEqual(TEXT("Independent Proxy executes while observed Stage remains zero"), Executed[1].Load(), 1);
	Scheduler.SetObservedTerrainStage(1);
	const double CoarseDeadline = FPlatformTime::Seconds() + 5.0;
	while (Applied[1] == 0 && FPlatformTime::Seconds() < CoarseDeadline)
	{
		Scheduler.Tick(Apply, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestEqual(TEXT("Independent Proxy resumes when the coarse stage opens"), Applied[1], 1);
	Interest.Admissions = {Admission};
	Manager.VoxelProxyReady.Add(Distant);
	const auto ReadyProxy = MakeShared<FVoxelVoxelProxyData, ESPMode::ThreadSafe>();
	ReadyProxy->Key = Distant;
	ReadyProxy->Geometry = {{EVoxelTerrainRepresentation::VoxelProxy, Distant.Coordinate, Distant.Level}, 2, -128, 256, 17, 23};
	Interest.Geometry.Add(ReadyProxy->Geometry.Key, ReadyProxy->Geometry);
	Manager.VoxelProxyData.Add(Distant, ReadyProxy);
	TestFalse(TEXT("Proxy fixture is ready but has not transferred visible ownership"), Manager.IsAdmissionPresented(Admission));
	FVoxelViewAdmission Surface;
	Surface.Kind = EVoxelViewAdmissionKind::Surface;
	Interest.Admissions.Add(Surface);
	Manager.bTerrainBuildStageDirty = true;
	Manager.RefreshTerrainBuildStage();
	TestEqual(TEXT("Ready Proxy permits its Surface replacement to build before ownership transfer"), Scheduler.GetObservedTerrainStage(), 2);
	FVoxelViewAdmission Fine;
	Fine.Kind = EVoxelViewAdmissionKind::Fine;
	Interest.Admissions.Add(Fine);
	Manager.FineReady.Add(Fine.FineKey);
	Manager.bTerrainBuildStageDirty = true;
	Manager.RefreshTerrainBuildStage();
	TestEqual(TEXT("Ready but unpresented Fine still holds the playable stage gate"), Scheduler.GetObservedTerrainStage(), 0);
	Manager.VisibleTerrainNodes.Add({Fine.FineKey, 0});
	Manager.PresentedFineSectionOwners.Add(Fine.FineKey, {Fine.FineKey, 0});
	Manager.bTerrainBuildStageDirty = true;
	Manager.RefreshTerrainBuildStage();
	TestEqual(TEXT("Derived ownership still requires a Native Fine receipt before opening the stage"), Scheduler.GetObservedTerrainStage(), 0);
	Scheduler.StopAndJoin();
	Manager.Reset();
	return true;
}

#endif
