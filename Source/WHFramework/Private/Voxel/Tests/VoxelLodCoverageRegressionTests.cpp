#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Geometry/DWVoxelBoundaryTransition.h"
#include "Voxel/VoxelModule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelActualSamplingLodTest,
	"WHFramework.Voxel.Rendering.ActualSamplingLOD",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelActualSamplingLodTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const int32 Steps[] = {1, 1, 2, 4, 8, 16, 16};
	for (uint8 Level = 0; Level < UE_ARRAY_COUNT(Steps); ++Level)
	{
		const FVoxelViewKey Key{{}, Level};
		TestEqual(TEXT("Actual sampling grows through L5 then saturates"), Key.GetSampleStep(), Steps[Level]);
		TestEqual(TEXT("Grid and actual sampling cover the entire node"), Key.GetGridSide() * Key.GetSampleStep(), Key.GetSide());
	}
	FVoxelStreamingSource Source;
	Source.Capabilities = EVoxelStreamingCapability::WorldVisual;
	Source.VerticalFovDegrees = 90.0f;
	Source.ViewportHeightPixels = 600;
	Source.View.TargetScreenErrorPixels = 8.0f;
	Source.View.VoxelProxyRadiusCells = 4096;
	TestEqual(TEXT("A 16-cell sample fails immediately before its pixel threshold"),
		VoxelViewLod::ResolveVoxelProxyLevel(599, Source, 5, 8.0f), static_cast<uint8>(4));
	TestEqual(TEXT("L5 becomes eligible at its actual 16-cell threshold"),
		VoxelViewLod::ResolveVoxelProxyLevel(601, Source, 5, 8.0f), static_cast<uint8>(5));
	TestEqual(TEXT("Saturated L6 is evaluated as 16 cells rather than an imaginary 32"),
		VoxelViewLod::ResolveVoxelProxyLevel(601, Source, 6, 8.0f), static_cast<uint8>(6));
	TestEqual(TEXT("Texture period is capped independently at eight cells"), VoxelViewLod::TexturePeriodCells(16, 8.0), 8.0);
	FVoxelTerrainViewPlan Plan;
	const FVoxelViewKey Root{FIntVector(2, 0, 0), 5};
	Plan.Build(MakeArrayView(&Source, 1), {}, {Root}, FVoxelViewSettings(), 8192);
	TestTrue(TEXT("Eligible 512-cell node remains a planned leaf"), Plan.Leaves.Num() == 1 && Plan.Leaves.Contains(Root));
	Source.Center.X = 425;
	Plan.Build(MakeArrayView(&Source, 1), {}, {Root}, FVoxelViewSettings(), 8192);
	TestFalse(TEXT("512-cell coverage refines when its actual sample exceeds screen error"), Plan.Leaves.Contains(Root));
	FString Error;
	TestTrue(TEXT("Refinement preserves complete ownership"), Plan.Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPublisherStageAndIndexTest,
	"WHFramework.Voxel.Rendering.PublisherStageAndIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPublisherStageAndIndexTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler;
	Scheduler.SetTerrainBuildStage(3);
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
	Publisher.AdmitCoveragePreparation(Publication);
	TestTrue(TEXT("Uncovered initial comparison completes synchronously"), Publication->bCoveragePrepared);
	// 覆盖精化重排 owner，先前批次中的 Source 不能留在旧物理索引。
	Publisher.BuildGroups(Publication, true);
	Publisher.SortGroups(Publication);
	Publication->bGroupsPrepared = true;
	Publisher.AdmitBuilds(Publication);
	for (int32 Index = 0; Index < Publication->Updates.Num(); ++Index)
		TestTrue(TEXT("Every prepared direct mesh still belongs to its final update"),
			Publication->PreparedMeshes[Index] == Publication->Updates[Index].Source);
	Scheduler.SetTerrainBuildStage(0);
	for (const auto& Update : Publication->Updates)
	{
		const auto& Group = Publication->Groups[Update.GroupIndex];
		if (Update.TerrainStage != 0)
		{
			TestFalse(TEXT("Independent distant mesh retains its later-stage gate"), Publisher.CanPublishGroup(Group));
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
	TMap<FVoxelPublishGroupKey, FVoxelViewPublisher::FGroupPriority> DeferredPriorities;
	Publisher.DeferSaturatedGroups(Demand, DeferredPriorities);
	TestTrue(TEXT("Third Fine generation moves back to the pending area"), Demand->Groups.IsEmpty());
	Publisher.UpdateGroupPriorities(MoveTemp(DeferredPriorities));
	TestTrue(TEXT("Deferred Fine donates Stage zero to its spatial Surface predecessor"),
		Publisher.CanPublishGroup(Older->Groups[1]));
	TestFalse(TEXT("Independent Surface remains behind its original stage gate"),
		Publisher.CanPublishGroup(Older->Groups[2]));
	TestFalse(TEXT("Deferred demand never promotes a newer spatial publication"),
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
	(void)InParameters;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler;
	Scheduler.SetTerrainBuildStage(3);
	FVoxelViewManager Manager(*Module, Scheduler, 7);
	const FVoxelViewKey Node{FIntVector::ZeroValue, 1};
	const FVoxelPublishGroupKey Owner{1, FIntPoint::ZeroValue, 1};
	auto& Interest = const_cast<FVoxelInterestSet&>(Module->GetCurrentInterest());
	Interest.TerrainPlan.Roots = {Node};
	Interest.TerrainPlan.Leaves = {Node};
	Interest.TerrainPlan.Required = {Node};
	Manager.VoxelProxyWanted.Add(Node);
	Manager.VoxelProxyReady.Add(Node);
	Manager.CommitTerrainOwnership(Owner, MakeArrayView(&Node, 1));
	auto Data = MakeShared<FVoxelVoxelProxyData>();
	Data->Key = Node;
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
	Manager.PreparedVolumeMeshes.Add(Node, MakeShared<FVoxelSectionMeshResult>());
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
	(void)InParameters;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FVoxelTaskScheduler Scheduler;
	Scheduler.SetTerrainBuildStage(0);
	FVoxelViewManager Manager(*Module, Scheduler, 11);
	const FVoxelViewKey Dependency{FIntVector::ZeroValue, 1};
	const FVoxelViewKey Distant{FIntVector(10, 0, 0), 1};
	const FVoxelViewKey Root{FIntVector::ZeroValue, 5};
	auto& Interest = const_cast<FVoxelInterestSet&>(Module->GetCurrentInterest());
	Interest.TerrainPlan.Build({}, {FIntVector::ZeroValue}, {Root}, FVoxelViewSettings(), 8192);
	TestTrue(TEXT("Real terrain plan preserves the Fine ancestor dependency"),
		Interest.TerrainPlan.FineDependencies.Contains(Dependency));
	TestTrue(TEXT("Real terrain plan preserves the coarse sibling needed for atomic handoff"),
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
	while (Applied[0] == 0 && FPlatformTime::Seconds() < FineDeadline)
	{
		Scheduler.Tick(Apply, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestEqual(TEXT("Fine-dependent Proxy builds and applies through Stage zero"), Applied[0], 1);
	TestEqual(TEXT("Independent Proxy cannot execute ahead of its stage"), Executed[1].Load(), 0);
	Scheduler.SetTerrainBuildStage(1);
	const double CoarseDeadline = FPlatformTime::Seconds() + 5.0;
	while (Applied[1] == 0 && FPlatformTime::Seconds() < CoarseDeadline)
	{
		Scheduler.Tick(Apply, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestEqual(TEXT("Independent Proxy resumes when the coarse stage opens"), Applied[1], 1);
	Interest.Admissions = {Admission};
	Manager.VoxelProxyReady.Add(Distant);
	TestFalse(TEXT("Proxy fixture is ready but has not transferred visible ownership"), Manager.IsAdmissionPresented(Admission));
	FVoxelViewAdmission Surface;
	Surface.Kind = EVoxelViewAdmissionKind::Surface;
	Interest.Admissions.Add(Surface);
	Manager.bTerrainBuildStageDirty = true;
	Manager.RefreshTerrainBuildStage();
	TestEqual(TEXT("Ready Proxy permits its Surface replacement to build before ownership transfer"), Scheduler.GetTerrainBuildStage(), 2);
	FVoxelViewAdmission Fine;
	Fine.Kind = EVoxelViewAdmissionKind::Fine;
	Interest.Admissions.Add(Fine);
	Manager.FineReady.Add(Fine.FineKey);
	Manager.bTerrainBuildStageDirty = true;
	Manager.RefreshTerrainBuildStage();
	TestEqual(TEXT("Ready but unpresented Fine still holds the playable stage gate"), Scheduler.GetTerrainBuildStage(), 0);
	Manager.VisibleTerrainNodes.Add({Fine.FineKey, 0});
	Manager.bTerrainBuildStageDirty = true;
	Manager.RefreshTerrainBuildStage();
	TestEqual(TEXT("Presented Fine releases dependent coarse construction"), Scheduler.GetTerrainBuildStage(), 2);
	Scheduler.StopAndJoin();
	Manager.Reset();
	return true;
}

#endif
