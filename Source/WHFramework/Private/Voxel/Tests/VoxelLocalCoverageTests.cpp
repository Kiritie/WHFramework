#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Main/MainModule.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Rendering/VoxelViewPublisher.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"
#include "Voxel/Rendering/DWVolumeTransitionPlanner.h"
#include "Voxel/Rendering/DWHeightfieldTransitionBuilder.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoverageRegionHaloTest,
	"WHFramework.Voxel.Rendering.CoverageRegionHalo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoverageRegionHaloTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	for (const int32 Offset : {0, -64})
	{
		const FVoxelViewKey Owner{FIntVector(31 + Offset / 2, 0, 0), 1};
		const FVoxelViewKey Boundary{FIntVector(64 + Offset, 0, 0), 0};
		const FVoxelViewKey Interior{FIntVector(80 + Offset, 0, 0), 0};
		const TSet<FVoxelViewKey> Nodes{Owner, Boundary, Interior};
		TMap<FVoxelViewKey, TSet<FVoxelViewKey>> Regions;
		FVoxelVolumeTransitionPlanner::GatherCoverageRegions(Nodes, Regions);
		FVoxelViewKey Region = Owner;
		while (Region.Level < 6) Region = Region.GetParent();
		const auto& Local = Regions.FindChecked(Region);
		TestTrue(TEXT("Local closure retains the owned node and the adjacent fine face"), Local.Contains(Owner) && Local.Contains(Boundary));
		TestFalse(TEXT("Neighbor interior does not invalidate local topology"), Local.Contains(Interior));
		TArray<FVoxelVolumeTransitionFace> GlobalFaces;
		TArray<FVoxelVolumeTransitionFace> LocalFaces;
		FVoxelVolumeTransitionPlanner::Build(Nodes, 6, GlobalFaces);
		FVoxelVolumeTransitionPlanner::Build(Local, 6, LocalFaces);
		TestEqual(TEXT("Thin halo produces every global mixed LOD face"), LocalFaces.Num(), GlobalFaces.Num());
		TestEqual(TEXT("Fixture crosses a region boundary"), GlobalFaces.Num(), 1);
		for (int32 Index = 0; Index < FMath::Min(GlobalFaces.Num(), LocalFaces.Num()); ++Index)
		{
			TestTrue(TEXT("Local seam retains owner, neighbor and direction"),
				LocalFaces[Index].Owner == GlobalFaces[Index].Owner && LocalFaces[Index].Neighbor == GlobalFaces[Index].Neighbor &&
				LocalFaces[Index].Direction == GlobalFaces[Index].Direction);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelVolumeRegionFaceFilterTest,
	"WHFramework.Voxel.Rendering.VolumeRegionFaceFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelVolumeRegionFaceFilterTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	// The old production path built every face, sorted it, and only then removed
	// owners outside the region. Compare the full ordered payload, not a hash.
	const auto CompareToPrevious = [this](const FString& Case, const TSet<FVoxelViewKey>& Nodes,
		const uint8 MaximumLevel, const TMap<FVoxelViewKey, int32>* Steps, const FVoxelViewKey* Region)
	{
		TArray<FVoxelVolumeTransitionFace> Expected;
		TArray<FVoxelVolumeTransitionFace> Actual;
		FVoxelVolumeTransitionPlanner::Build(Nodes, MaximumLevel, Expected, Steps, nullptr);
		if (Region)
		{
			Expected.RemoveAll([Region](const FVoxelVolumeTransitionFace& Face)
			{
				FVoxelViewKey Owner = Face.Owner;
				while (Owner.Level < Region->Level) Owner = Owner.GetParent();
				return !(Owner == *Region);
			});
		}
		FVoxelVolumeTransitionPlanner::Build(Nodes, MaximumLevel, Actual, Steps, Region);
		TestEqual(Case + TEXT(" ordered face count"), Actual.Num(), Expected.Num());
		for (int32 Index = 0; Index < FMath::Min(Actual.Num(), Expected.Num()); ++Index)
		{
			const auto& A = Actual[Index];
			const auto& E = Expected[Index];
			TestTrue(FString::Printf(TEXT("%s ordered face %d retains every field"), *Case, Index),
				A.Owner == E.Owner && A.Neighbor == E.Neighbor && A.Direction == E.Direction &&
				A.Min == E.Min && A.Max == E.Max && A.Ratio == E.Ratio);
		}
		return Actual;
	};
	const auto HasFace = [](const TArray<FVoxelVolumeTransitionFace>& Faces,
		const FVoxelViewKey& Owner, const FVoxelViewKey& Neighbor)
	{
		return Faces.ContainsByPredicate([&](const FVoxelVolumeTransitionFace& Face)
		{
			return Face.Owner == Owner && Face.Neighbor == Neighbor;
		});
	};

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const FVoxelViewKey Owner{FIntVector(-1, -1, -1), 1};
		FIntVector NeighborCoordinate(-1, -1, -1);
		NeighborCoordinate[Axis] = 0;
		const FVoxelViewKey Neighbor{NeighborCoordinate, 1};
		const FVoxelViewKey OwnerRegion{FIntVector(-1, -1, -1), 2};
		FIntVector OtherRegionCoordinate(-1, -1, -1);
		OtherRegionCoordinate[Axis] = 0;
		const FVoxelViewKey OtherRegion{OtherRegionCoordinate, 2};
		const TSet<FVoxelViewKey> Nodes{Owner, Neighbor};
		const TMap<FVoxelViewKey, int32> Steps{{Owner, 4}, {Neighbor, 2}};
		const FString Case = FString::Printf(TEXT("negative coordinate axis %d"), Axis);
		const auto Faces = CompareToPrevious(Case, Nodes, 2, &Steps, &OwnerRegion);
		if (TestEqual(Case + TEXT(" produces an actual same-level face"), Faces.Num(), 1))
		{
			const FVoxelGenerationBounds Bounds = Neighbor.GetBounds();
			TestTrue(Case + TEXT(" has the expected owner, neighbor, direction, bounds and ratio"),
				Faces[0].Owner == Owner && Faces[0].Neighbor == Neighbor &&
				static_cast<uint8>(Faces[0].Direction) == uint8(Axis * 2) &&
				Faces[0].Min == Bounds.Min && Faces[0].Max == Bounds.Max && Faces[0].Ratio == 1);
		}
		TestTrue(Case + TEXT(" excludes the other owner region"),
			CompareToPrevious(Case + TEXT(" outside"), Nodes, 2, &Steps, &OtherRegion).IsEmpty());
		const TMap<FVoxelViewKey, int32> EqualSteps{{Owner, 2}, {Neighbor, 2}};
		TestTrue(Case + TEXT(" equal steps do not produce a transition"),
			CompareToPrevious(Case + TEXT(" equal steps"), Nodes, 2, &EqualSteps, &OwnerRegion).IsEmpty());
		const TMap<FVoxelViewKey, int32> MissingStep{{Owner, 4}};
		TestTrue(Case + TEXT(" missing neighbor step does not produce a transition"),
			CompareToPrevious(Case + TEXT(" missing step"), Nodes, 2, &MissingStep, &OwnerRegion).IsEmpty());
	}

	const FVoxelViewKey Fine{FIntVector(-1, 0, 0), 0};
	const FVoxelViewKey FirstCoarse{FIntVector::ZeroValue, 1};
	const FVoxelViewKey LaterCoarse{FIntVector::ZeroValue, 2};
	const FVoxelViewKey CoarseRegion{FIntVector::ZeroValue, 2};
	const FVoxelViewKey OutsideRegion{FIntVector(-1, 0, 0), 2};
	TSet<FVoxelViewKey> Mixed{Fine, FirstCoarse, LaterCoarse};
	const auto MixedFaces = CompareToPrevious(TEXT("first cross-LOD neighbor"), Mixed, 2, nullptr, &CoarseRegion);
	TestTrue(TEXT("The first cross-LOD owner produces a real transition"), HasFace(MixedFaces, FirstCoarse, Fine));
	TestFalse(TEXT("A later overlapping coarse candidate does not replace the first"), HasFace(MixedFaces, LaterCoarse, Fine));
	const auto FilteredMixed = CompareToPrevious(TEXT("first cross-LOD owner filtered"), Mixed, 2, nullptr, &OutsideRegion);
	TestFalse(TEXT("A first owner outside the region contributes no fine transition"),
		HasFace(FilteredMixed, FirstCoarse, Fine) || HasFace(FilteredMixed, LaterCoarse, Fine));
	TestTrue(TEXT("An owner coarser than the requested region is not included"),
		CompareToPrevious(TEXT("owner above region level"), Mixed, 2, nullptr, &Fine).IsEmpty());
	Mixed.Add({FIntVector::ZeroValue, 0});
	const auto SameLevelFirst = CompareToPrevious(TEXT("same-level neighbor precedes coarse search"), Mixed, 2, nullptr, &CoarseRegion);
	TestFalse(TEXT("A same-level neighbor suppresses the coarse candidates for that fine node"),
		HasFace(SameLevelFirst, FirstCoarse, Fine) || HasFace(SameLevelFirst, LaterCoarse, Fine));

	TSet<FVoxelViewKey> Neighborhood;
	TMap<FVoxelViewKey, int32> NeighborhoodSteps;
	const FVoxelViewKey Center{FIntVector::ZeroValue, 6};
	for (int32 Z = -1; Z <= 1; ++Z)
		for (int32 Y = -1; Y <= 1; ++Y)
			for (int32 X = -1; X <= 1; ++X)
			{
				const FVoxelViewKey Key{FIntVector(X, Y, Z), 6};
				Neighborhood.Add(Key);
				NeighborhoodSteps.Add(Key, Key == Center ? 4 : 2);
			}
	TMap<FVoxelViewKey, TSet<FVoxelViewKey>> Regions;
	FVoxelVolumeTransitionPlanner::GatherCoverageRegions(Neighborhood, Regions);
	const TSet<FVoxelViewKey>* CenterLocal = Regions.Find(Center);
	if (TestNotNull(TEXT("The central region exists"), CenterLocal))
	{
		TestEqual(TEXT("The central region retains all 26 touching neighbors"), CenterLocal->Num(), 27);
		const auto CenterFaces = CompareToPrevious(TEXT("26 neighboring regions"), *CenterLocal, 6, &NeighborhoodSteps, &Center);
		TestEqual(TEXT("Six face neighbors yield six central owner transitions"), CenterFaces.Num(), 6);
		for (const auto& Face : CenterFaces)
		{
			const FIntVector Delta = Face.Neighbor.Coordinate - Center.Coordinate;
			TestTrue(TEXT("Each central transition reaches one face neighbor"), Face.Owner == Center &&
				FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) + FMath::Abs(Delta.Z) == 1 && Face.Ratio == 1);
		}
	}
	for (const auto& Pair : Regions)
	{
		CompareToPrevious(TEXT("each neighboring owner region"), Pair.Value, 6, &NeighborhoodSteps, &Pair.Key);
	}
	const auto AllFaces = CompareToPrevious(TEXT("no owner filter"), Neighborhood, 6, &NeighborhoodSteps, nullptr);
	TestEqual(TEXT("Unfiltered neighborhood retains all six observable faces"), AllFaces.Num(), 6);
	TArray<FVoxelVolumeTransitionFace> DefaultFaces;
	FVoxelVolumeTransitionPlanner::Build(Neighborhood, 6, DefaultFaces, &NeighborhoodSteps);
	TestEqual(TEXT("Omitting the region parameter preserves the default result count"), DefaultFaces.Num(), AllFaces.Num());
	for (int32 Index = 0; Index < FMath::Min(DefaultFaces.Num(), AllFaces.Num()); ++Index)
	{
		const auto& A = DefaultFaces[Index];
		const auto& E = AllFaces[Index];
		TestTrue(TEXT("Default region preserves the ordered face payload"),
			A.Owner == E.Owner && A.Neighbor == E.Neighbor && A.Direction == E.Direction &&
			A.Min == E.Min && A.Max == E.Max && A.Ratio == E.Ratio);
	}
	const TSet<FVoxelViewKey> EmptyNodes;
	TArray<FVoxelVolumeTransitionFace> ResetFaces = AllFaces;
	FVoxelVolumeTransitionPlanner::Build(EmptyNodes, 6, ResetFaces, nullptr, &Center);
	TestTrue(TEXT("Empty input clears previously produced output"), ResetFaces.IsEmpty());
	TestTrue(TEXT("Empty input matches the previous region filter"),
		CompareToPrevious(TEXT("empty input"), EmptyNodes, 6, nullptr, &Center).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelManagerLocalCoverageTest,
	"WHFramework.Voxel.Rendering.ManagerLocalCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelManagerLocalCoverageTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UVoxelModule* Module = NewObject<UVoxelModule>();
	FString FixtureError;
	if (!Module->Registry.Build({}, false, FixtureError)) return false;
	const auto Shapes = MakeShared<FVoxelShapeRegistry, ESPMode::ThreadSafe>();
	Shapes->BuildDefaults();
	Module->Shapes = Shapes;
	Module->Generator = VoxelTest::MakeGenerator();
	Module->Epoch = 7;
	Module->GenerationConfig = VoxelTest::MakeGenerationConfig();
	Module->Manifest.WorldId = FGuid::NewGuid();
	Module->Manifest.RegistryHash = Module->GetRegistry()->Hash;
	Module->Manifest.RecipeHash = Module->GenerationConfig->Recipe->RecipeHash;
	Module->Manifest.Settings = Module->GenerationConfig->Recipe->Settings;
	Module->Manifest.BaseSampleHash = VoxelBinary::Mix64(FVoxelManifestCodec::RecipeFingerprint(Module->Manifest) ^
		VoxelBinary::Mix64(Module->Manifest.RegistryHash) ^ VoxelBinary::Mix64(Module->Manifest.RecipeHash));
	const auto CreateScheduler = [this](const uint64 Epoch, TUniquePtr<FVoxelTaskScheduler>& OutScheduler)
	{
		const auto Control = FVoxelGenerationConsumerKey::Section(Epoch, FGuid::NewGuid(), 1, {}, 1);
		TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> GeometryResources, FactResources;
		FString Error;
		return TestTrue(TEXT("Coverage fixture admits its original ledgers and scheduler identity"),
			FVoxelTerrainResources::Create({}, Control, GeometryResources, Error) &&
			FVoxelTerrainResources::Create({}, Control, FactResources, Error) &&
			FVoxelTaskScheduler::Create(GeometryResources.ToSharedRef(), FactResources.ToSharedRef(), Control, OutScheduler, Error) &&
			OutScheduler->InitializeStatistics(Epoch));
	};
	TUniquePtr<FVoxelTaskScheduler> SchedulerOwner;
	if (!CreateScheduler(7, SchedulerOwner)) return false;
	auto& Scheduler = *SchedulerOwner;
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 2;
	Budget.MaxCompletedResultsPerFrame = 1;
	Scheduler.SetBudget(Budget);
	FVoxelViewManager Manager(*Module, Scheduler, 7);
	const auto EnsureProxyInputs = [this, Module, &Scheduler, &Manager](const TSet<FVoxelViewKey>& InNodes)
	{
		auto& Interest = const_cast<FVoxelInterestSet&>(Module->GetCurrentInterest());
		for (const auto& Key : InNodes)
		{
			if (Manager.VoxelProxyData.Contains(Key)) continue;
			const auto Consumer = FVoxelGenerationConsumerKey::Section(7, FGuid::NewGuid(), 1, Key.Coordinate, 1);
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
			const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelVoxelProxyData>() +
				VoxelBoundedAllocation::ArrayBytes<FVoxelBlockState>(4096) +
				6 * VoxelBoundedAllocation::ArrayBytes<FVoxelBlockState>(256);
			if (!TestTrue(TEXT("Frozen coarse cells and halos acquire their original resident lease"),
				Scheduler.GetTerrainResources()->Acquire(Consumer, EVoxelTerrainResourcePool::Resident, Bytes, Lease))) return false;
			auto Data = VoxelBoundedAllocation::MakeWorkerShared<FVoxelVoxelProxyData>(Lease);
			Data->ResourceLease = Lease;
			Data->Key = Key;
			Data->Geometry = {{EVoxelTerrainRepresentation::VoxelProxy, Key.Coordinate, Key.Level}, 2,
				Module->Manifest.Settings.MinZ, Module->Manifest.Settings.MaxZ,
				Module->Manifest.BaseSampleHash, Module->GetRegistry()->Hash};
			Data->Revision = 1;
			Data->Cells.Init(FVoxelBlockState(), 4096);
			for (int32 Face = 0; Face < 6; ++Face)
			{
				Data->Known[Face] = true;
				Data->Halo[Face].Init(FVoxelBlockState(), 256);
			}
			Interest.Geometry.Add(Data->Geometry.Key, Data->Geometry);
			Manager.VoxelProxyData.Add(Key, MoveTemp(Data));
		}
		return true;
	};
	const auto PrepareTransitions = [this, Module, &Scheduler, &Manager](const TSet<FVoxelViewKey>& InNodes)
	{
		const double PrepareDeadline = FPlatformTime::Seconds() + 10.0;
		while (FPlatformTime::Seconds() < PrepareDeadline)
		{
			Manager.RebuildVolumeTransitions(InNodes);
			bool bPrepared = true;
			for (const auto& Key : InNodes)
			{
				const uint64 Desired = Manager.DesiredVolumeSignatures.FindRef(Key);
				bPrepared &= Desired != 0 && Manager.PreparedVolumeSignatures.FindRef(Key) == Desired &&
					Manager.PreparedVolumeMeshes.Contains(Key);
			}
			if (bPrepared) return true;
			auto& Frame = Module->GetTerrainFrameContext();
			Frame.Begin(7, Frame.GetFrameCounter() + 1, FPlatformTime::Seconds(), {});
			Manager.PumpVolumeTransitions();
			Frame.End(FPlatformTime::Seconds());
			Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
			FPlatformProcess::Sleep(0.001f);
		}
		return TestTrue(TEXT("Actual frozen coarse inputs prepare every local body and boundary through the scheduler"), false);
	};
	const TSet<FVoxelViewKey> FineNodes{{FIntVector::ZeroValue, 0}, {FIntVector(1000, 0, 0), 0}};
	Manager.RebuildVolumeTransitions(FineNodes);
	TestTrue(TEXT("Native Fine ownership does not enqueue redundant coarse topology plans"), Manager.VolumeCoveragePlans.IsEmpty());
	TestEqual(TEXT("Fine-only topology does not occupy a coarse planning worker"), Scheduler.ActiveCount(), 0);
	const TSet<FVoxelViewKey> Nodes{{FIntVector(2, 0, 0), 1}, {FIntVector(1000, 0, 0), 1}};
	Manager.RebuildVolumeTransitions(Nodes);
	TestEqual(TEXT("Distant regions have independent coverage jobs"), Manager.VolumeCoveragePlans.Num(), 2);
	int32 Ready = 0;
	const double Deadline = FPlatformTime::Seconds() + 10.0;
	while (Ready == 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		Ready = 0;
		for (const auto& Pair : Manager.VolumeCoveragePlans) Ready += Pair.Value.Plan.IsValid() ? 1 : 0;
		if (Ready == 0) FPlatformProcess::Sleep(0.001f);
	}
	if (!TestEqual(TEXT("One completion can be applied while the other remains pending"), Ready, 1))
	{
		Scheduler.StopAndJoin();
		return false;
	}
	Manager.RebuildVolumeTransitions(Nodes);
	for (const auto& Pair : Manager.VolumeCoveragePlans)
	{
		const FVoxelGenerationBounds Bounds = Pair.Key.GetBounds();
		const FVoxelCoverageBox Box{Bounds.Min, Bounds.Max};
		TestTrue(TEXT("A topology proof alone cannot release a region with missing frozen coarse inputs"),
			VoxelCoverage::IntersectsAny3D(Box, Manager.PendingVolumeHandoffBoxes));
	}
	while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	Manager.RebuildVolumeTransitions(Nodes);
	TestFalse(TEXT("Completed topology still waits for actual body and frozen boundary preparation"), Manager.PendingVolumeHandoffBoxes.IsEmpty());
	if (!EnsureProxyInputs(Nodes) || !PrepareTransitions(Nodes)) { Scheduler.StopAndJoin(); return false; }
	TestTrue(TEXT("Prepared regions release all local handoff waits"), Manager.PendingVolumeHandoffBoxes.IsEmpty());
	const FVoxelViewKey ReadOnlyRegion{FIntVector(31, 0, 0), 6};
	const uint64 DistantSignature = Manager.VolumeCoveragePlans.FindChecked(ReadOnlyRegion).DesiredSignature;
	Manager.bActiveCoverageFull = false;
	Manager.ActiveDirtyCoverageRects = {{FIntPoint::ZeroValue, FIntPoint(1024, 1024)}};
	TSet<FVoxelViewKey> WithReadOnlyNeighbors = Nodes;
	WithReadOnlyNeighbors.Add({FIntVector(1001, 0, 0), 1});
	Manager.RebuildVolumeTransitions(WithReadOnlyNeighbors);
	TestEqual(TEXT("Read-only neighboring input cannot replace a region outside the update scope"),
		Manager.VolumeCoveragePlans.FindChecked(ReadOnlyRegion).DesiredSignature, DistantSignature);
	TestEqual(TEXT("Unchanged neighboring ownership does not enqueue another plan"),
		Manager.VolumeCoveragePlans.FindChecked(ReadOnlyRegion).PendingSignature, uint64(0));
	Manager.bActiveCoverageFull = true;
	Manager.ActiveDirtyCoverageRects.Reset();
	TMap<FVoxelViewKey, TSet<FVoxelViewKey>> Before, After;
	FVoxelVolumeTransitionPlanner::GatherCoverageRegions(Nodes, Before);
	TSet<FVoxelViewKey> Changed = Nodes;
	Changed.Add({FIntVector(1, 0, 0), 1});
	FVoxelVolumeTransitionPlanner::GatherCoverageRegions(Changed, After);
	const FVoxelViewKey Distant{FIntVector(31, 0, 0), 6};
	TestTrue(TEXT("A local edit preserves distant planning inputs"),
		Before.Contains(Distant) && After.Contains(Distant) && Before[Distant].Includes(After[Distant]) && Before[Distant].Num() == After[Distant].Num());
	const FVoxelViewKey LocalRegion{FIntVector::ZeroValue, 6};
	const auto InitialPlan = Manager.VolumeCoveragePlans.FindChecked(LocalRegion).Plan;
	Changed = Nodes;
	Changed.Add({FIntVector(8, 0, 0), 1});
	Manager.RebuildVolumeTransitions(Changed);
	const uint64 PendingSignature = Manager.VolumeCoveragePlans.FindChecked(LocalRegion).PendingSignature;
	Changed.Add({FIntVector(9, 0, 0), 1});
	Manager.RebuildVolumeTransitions(Changed);
	TestTrue(TEXT("Changing topology retains its in-flight immutable plan"), PendingSignature != 0 &&
		Manager.VolumeCoveragePlans.FindChecked(LocalRegion).PendingSignature == PendingSignature);
	TestFalse(TEXT("Unchanged local Fine remains independent of topology changes"),
		VoxelCoverage::IntersectsAny3D(Manager.FineCoverageBox(FIntVector::ZeroValue), Manager.PendingVolumeHandoffBoxes));
	while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("Completed immutable input is accepted after desired topology changes"),
		Manager.VolumeCoveragePlans.FindChecked(LocalRegion).Plan != InitialPlan);
	Manager.RebuildVolumeTransitions(Changed);
	TestFalse(TEXT("Completed inputs do not block the unchanged local Fine"),
		VoxelCoverage::IntersectsAny3D(Manager.FineCoverageBox(FIntVector::ZeroValue), Manager.PendingVolumeHandoffBoxes));
	while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	Manager.RebuildVolumeTransitions(Changed);
	if (!EnsureProxyInputs(Changed) || !PrepareTransitions(Changed)) { Scheduler.StopAndJoin(); return false; }
	TestTrue(TEXT("Coalesced latest topology eventually releases all handoffs"), Manager.PendingVolumeHandoffBoxes.IsEmpty());
	Changed.Add({FIntVector(0, 0, 16), 1});
	Manager.RebuildVolumeTransitions(Changed);
	TestFalse(TEXT("A topology change at another height cannot block ground Fine at the same XY"),
		VoxelCoverage::IntersectsAny3D(Manager.FineCoverageBox(FIntVector::ZeroValue), Manager.PendingVolumeHandoffBoxes));
	while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	const TSet<FVoxelViewKey> Column{{FIntVector(0, 0, 0), 1}, {FIntVector(0, 0, 1), 1}};
	if (!EnsureProxyInputs(Column)) { Scheduler.StopAndJoin(); return false; }
	Manager.RebuildVolumeTransitions(Column);
	while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	Manager.RebuildVolumeTransitions(Column);
	TestEqual(TEXT("Same-column owners can all prepare before any publication"), Manager.DesiredVolumeSignatures.Num(), 2);
	const FVoxelViewKey Prepared{FIntVector(0, 0, 0), 1};
	if (!PrepareTransitions(Column)) { Scheduler.StopAndJoin(); return false; }
	Manager.RebuildVolumeTransitions(Column);
	TestTrue(TEXT("Another unpublished owner cannot discard a prepared column result"), Manager.PreparedVolumeSignatures.Contains(Prepared));
	Scheduler.StopAndJoin();
	Manager.Reset();
	const FVoxelPublishGroupKey ColumnOwner{0, FIntPoint(-7, 5), 0};
	const FVoxelPublishGroupKey DistantOwner{0, FIntPoint(1000, 5), 0};
	const FVoxelViewKey Original{FIntVector(-7, 5, 0), 0};
	const FVoxelViewKey Intermediate{FIntVector(-7, 5, 1), 0};
	const FVoxelViewKey Latest{FIntVector(-7, 5, 2), 0};
	const FVoxelViewKey DistantNode{FIntVector(1000, 5, 0), 0};
	const TArray<FVoxelViewKey> OlderSnapshot{Intermediate}, NewerSnapshot{Latest};
	Manager.CommitTerrainOwnership(ColumnOwner, MakeArrayView(&Original, 1));
	Manager.CommitTerrainOwnership(DistantOwner, MakeArrayView(&DistantNode, 1));
	Manager.CommitTerrainOwnership(ColumnOwner, OlderSnapshot);
	Manager.CommitTerrainOwnership(ColumnOwner, NewerSnapshot);
	TestTrue(TEXT("A later immutable ownership snapshot replaces nodes committed by its predecessor"),
		Manager.VisibleTerrainNodes.Contains(Latest) && !Manager.VisibleTerrainNodes.Contains(Original) &&
		!Manager.VisibleTerrainNodes.Contains(Intermediate));
	TestTrue(TEXT("Replacing a column preserves an independent distant owner"), Manager.VisibleTerrainNodes.Contains(DistantNode));
	Manager.CommitTerrainOwnership(ColumnOwner, {});
	TestFalse(TEXT("An empty owner target retires its latest committed vertical nodes"), Manager.VisibleTerrainNodes.Contains(Latest));
	TestFalse(TEXT("An empty target releases its derived owner lookup"), Manager.VisibleTerrainNodesByGroup.Contains(ColumnOwner));
	Manager.Reset();
	TestTrue(TEXT("Reset releases both committed nodes and their owner index"),
		Manager.VisibleTerrainNodes.IsEmpty() && Manager.VisibleTerrainNodesByGroup.IsEmpty());
	const auto SameRect = [](const FVoxelCoverageRect& Left, const FVoxelCoverageRect& Right)
	{
		return Left.Min == Right.Min && Left.Max == Right.Max;
	};
	const FVoxelCoverageRect First{{0, 0}, {10, 10}};
	const FVoxelCoverageRect Second{{100, 0}, {110, 10}};
	const FVoxelCoverageRect Third{{200, 0}, {210, 10}};
	TArray<FVoxelCoverageRect> Pending{First, Second, Third};
	for (int32 Update = 0; Update < 8; ++Update)
	{
		FVoxelViewManager::MergeCoverageRect(Pending, First);
	}
	TestTrue(TEXT("Repeated updates retain the oldest overlapping scope before independent scopes"),
		Pending.Num() == 3 && SameRect(Pending[0], {{-1, -1}, {11, 11}}) &&
		SameRect(Pending[1], Second) && SameRect(Pending[2], Third));
	const FVoxelCoverageRect Fourth{{300, 0}, {310, 10}};
	FVoxelViewManager::MergeCoverageRect(Pending, Fourth);
	TestTrue(TEXT("A new independent scope is appended without reordering existing scopes"),
		Pending.Num() == 4 && SameRect(Pending[1], Second) && SameRect(Pending[2], Third) &&
		SameRect(Pending[3], {{299, -1}, {311, 11}}));
	TArray<FVoxelCoverageRect> Bridged{
		{{0, 0}, {10, 10}}, {{100, 100}, {110, 110}}, {{8, 12}, {18, 22}}, {{200, 200}, {210, 210}}};
	FVoxelViewManager::MergeCoverageRect(Bridged, {{16, 0}, {20, 14}});
	TestTrue(TEXT("A later intersection expands backwards to the earliest merged scope"),
		Bridged.Num() == 3 && SameRect(Bridged[0], {{0, -1}, {21, 22}}) &&
		SameRect(Bridged[1], {{100, 100}, {110, 110}}) && SameRect(Bridged[2], {{200, 200}, {210, 210}}));
	TArray<FVoxelCoverageRect> Empty;
	FVoxelViewManager::MergeCoverageRect(Empty, {{-64, -64}, {-48, -48}});
	TestTrue(TEXT("An empty queue keeps the original one-cell halo, including negative coordinates"),
		Empty.Num() == 1 && SameRect(Empty[0], {{-65, -65}, {-47, -47}}));
	TArray<FVoxelCoverageRect> Touching{{{0, 0}, {10, 10}}};
	FVoxelViewManager::MergeCoverageRect(Touching, {{11, 0}, {20, 10}});
	TestEqual(TEXT("Half-open scopes that only touch after the halo stay distinct"), Touching.Num(), 2);
	FVoxelViewManager::MergeCoverageRect(Touching, {{10, 0}, {11, 10}});
	TestTrue(TEXT("A halo intersection merges both adjacent scopes"), Touching.Num() == 1 &&
		SameRect(Touching[0], {{0, -1}, {21, 11}}));
	const auto RectLess = [](const FVoxelCoverageRect& Left, const FVoxelCoverageRect& Right)
	{
		if (Left.Min.X != Right.Min.X) return Left.Min.X < Right.Min.X;
		if (Left.Min.Y != Right.Min.Y) return Left.Min.Y < Right.Min.Y;
		if (Left.Max.X != Right.Max.X) return Left.Max.X < Right.Max.X;
		return Left.Max.Y < Right.Max.Y;
	};
	FRandomStream Random(1975);
	for (int32 Trial = 0; Trial < 128; ++Trial)
	{
		TArray<FVoxelCoverageRect> Actual;
		for (int32 Index = 0; Index < 16; ++Index)
		{
			const FIntPoint Min(Random.RandRange(-64, 64), Random.RandRange(-64, 64));
			Actual.Add({Min, Min + FIntPoint(Random.RandRange(1, 24), Random.RandRange(1, 24))});
		}
		TArray<FVoxelCoverageRect> Reference = Actual;
		const FIntPoint Min(Random.RandRange(-64, 64), Random.RandRange(-64, 64));
		const FVoxelCoverageRect Added{Min, Min + FIntPoint(Random.RandRange(1, 24), Random.RandRange(1, 24))};
		FVoxelCoverageRect OldMerged{Added.Min - FIntPoint(1, 1), Added.Max + FIntPoint(1, 1)};
		for (int32 Index = 0; Index < Reference.Num();)
		{
			const FVoxelCoverageRect& Existing = Reference[Index];
			if (!OldMerged.Intersects(Existing)) { ++Index; continue; }
			OldMerged.Min.X = FMath::Min(OldMerged.Min.X, Existing.Min.X);
			OldMerged.Min.Y = FMath::Min(OldMerged.Min.Y, Existing.Min.Y);
			OldMerged.Max.X = FMath::Max(OldMerged.Max.X, Existing.Max.X);
			OldMerged.Max.Y = FMath::Max(OldMerged.Max.Y, Existing.Max.Y);
			Reference.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			Index = 0;
		}
		Reference.Add(OldMerged);
		FVoxelViewManager::MergeCoverageRect(Actual, Added);
		Reference.Sort(RectLess);
		Actual.Sort(RectLess);
		if (!TestEqual(TEXT("Stable merge preserves the previous coverage rectangle count"), Actual.Num(), Reference.Num())) return false;
		for (int32 Index = 0; Index < Actual.Num(); ++Index)
		{
			if (!TestTrue(TEXT("Stable merge preserves every previous coverage rectangle"), SameRect(Actual[Index], Reference[Index]))) return false;
		}
	}
	Manager.bCoverageDirty = false;
	Manager.bCoverageDirtyFull = false;
	Manager.DirtyCoverageRects = {First};
	Manager.ActiveDirtyCoverageRects = {Second};
	Manager.MarkCoverageDirty({});
	TestTrue(TEXT("Invalid dirty bounds retain the existing full-coverage fallback without altering the active scope"),
		Manager.bCoverageDirty && Manager.bCoverageDirtyFull && Manager.DirtyCoverageRects.IsEmpty() &&
		Manager.ActiveDirtyCoverageRects.Num() == 1 && SameRect(Manager.ActiveDirtyCoverageRects[0], Second));
	Manager.ActiveDirtyCoverageRects.Reset();
	Manager.bCoverageDirty = false;
	Manager.bCoverageDirtyFull = false;
	FVoxelInterestDelta DataOnly;
	DataOnly.AddedExact.Add(FIntVector::ZeroValue);
	Manager.ApplyInterestDelta(DataOnly);
	TestFalse(TEXT("Data or priority revisions do not force visual coverage work"), Manager.bCoverageDirty);
	FVoxelInterestDelta VisualDelta;
	VisualDelta.bVisualPartitionChanged = true;
	VisualDelta.AddedFine.Add(FIntVector(-2, -2, 0));
	VisualDelta.RemovedFine.Add(FIntVector(-2, -2, 1));
	VisualDelta.AddedProxy.Add({FIntVector(1000, 0, 0), 1});
	VisualDelta.RemovedSurface.Add({FIntPoint(4000, 0), 0});
	VisualDelta.AddedMacro.Add({FIntPoint(100, 0), 0});
	Manager.ApplyInterestDelta(VisualDelta);
	TestTrue(TEXT("A visual delta dirties only its added and removed representation footprints"),
		Manager.bCoverageDirty && !Manager.bCoverageDirtyFull && Manager.DirtyCoverageRects.Num() == 4);
	Manager.bActiveCoverageFull = false;
	Manager.ActiveDirtyCoverageRects = Manager.DirtyCoverageRects;
	TestTrue(TEXT("Negative Fine additions and vertical removals share the same local scope"),
		Manager.CoverageAffects(Manager.FineCoverageRect(FIntVector(-2, -2, 0))));
	TestTrue(TEXT("A removed distant Surface is included in the coverage handoff"),
		Manager.CoverageAffects(Manager.SurfaceCoverageRect({FIntPoint(4000, 0), 0})));
	TestFalse(TEXT("Independent space between sparse changes is not refreshed"),
		Manager.CoverageAffects(Manager.FineCoverageRect(FIntVector(100, 100, 0))));
	Manager.ActiveDirtyCoverageRects = {{FIntPoint::ZeroValue, FIntPoint(16, 16)}};
	Manager.ExpandActiveCoverage(Manager.VoxelProxyCoverageRect({FIntVector(1000, 0, 0), 1}));
	TestTrue(TEXT("A discovered distant handoff extends the active scope without a global refresh"),
		!Manager.bActiveCoverageFull && Manager.CoverageAffects(Manager.FineCoverageRect(FIntVector::ZeroValue)) &&
		Manager.CoverageAffects(Manager.VoxelProxyCoverageRect({FIntVector(1000, 0, 0), 1})));
	TestFalse(TEXT("Extending a distant handoff preserves unrelated intermediate space"),
		Manager.CoverageAffects(Manager.FineCoverageRect(FIntVector(100, 100, 0))));
	Manager.Reset();
	TUniquePtr<FVoxelTaskScheduler> ScopedSchedulerOwner;
	if (!CreateScheduler(8, ScopedSchedulerOwner)) return false;
	auto& ScopedScheduler = *ScopedSchedulerOwner;
	Module->Epoch = 8;
	Module->Runtime = MakeUnique<FVoxelWorldRuntime>(8, true, Module->GetRegistry().ToSharedRef(),
		Module->Generator.ToSharedRef(), ScopedScheduler.GetRuntimeResources());
	ON_SCOPE_EXIT { ScopedScheduler.StopAndJoin(); Module->Runtime.Reset(); };
	FVoxelViewManager ScopedManager(*Module, ScopedScheduler, 8);
	auto& Interest = const_cast<FVoxelInterestSet&>(Module->GetCurrentInterest());
	const FVoxelViewKey NearRoot{FIntVector::ZeroValue, 1};
	const FVoxelViewKey FarRoot{FIntVector(100, 0, 0), 1};
	Interest.TerrainPlan.Roots = {NearRoot, FarRoot};
	Interest.TerrainPlan.Required = Interest.TerrainPlan.Roots;
	TArray<FVoxelViewKey> NearChildren;
	NearRoot.GetChildren(NearChildren);
	for (const auto& Child : NearChildren)
	{
		Interest.TerrainPlan.Leaves.Add(Child);
		Interest.TerrainPlan.Required.Add(Child);
		ScopedManager.FineReady.Add(Child.Coordinate);
		ScopedManager.GetInputRoot().FineWanted.Add(Child.Coordinate);
		const FVoxelFinePageKey Page{Child.Coordinate, 0};
		ScopedManager.GetInputRoot().FinePagePlan.Pages.Add(Page);
		ScopedManager.GetInputRoot().FinePagePlan.SectionOwners.Add(Child.Coordinate, Page);
		ScopedManager.IndexCoverage(Child);
	}
	Interest.TerrainPlan.Leaves.Add(FarRoot);
	ScopedManager.GetInputRoot().MaximumTerrainRootLevel = 1;
	for (const auto& Root : Interest.TerrainPlan.Roots)
	{
		auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
		Record->Geometry = {{EVoxelTerrainRepresentation::VoxelProxy, Root.Coordinate, Root.Level}, 2,
			Module->Manifest.Settings.MinZ, Module->Manifest.Settings.MaxZ, Module->Manifest.BaseSampleHash, Module->GetRegistry()->Hash};
		Interest.Geometry.Add(Record->Geometry.Key, Record->Geometry);
		const auto Bounds = Root.GetBounds(); Record->Min = Bounds.Min; Record->Max = Bounds.Max;
		FString IndexError;
		TestTrue(TEXT("Fixture registers each terrain root"), ScopedManager.GetInputRoot().TerrainRootIndex.Upsert(Record, IndexError));
		ScopedManager.GetInputRoot().IndexedTerrainRoots.Add(Root);
	}
	ScopedManager.CommitTerrainOwnership({1, FIntPoint(NearRoot.Coordinate.X, NearRoot.Coordinate.Y), NearRoot.Level}, MakeArrayView(&NearRoot, 1));
	ScopedManager.CommitTerrainOwnership({1, FIntPoint(FarRoot.Coordinate.X, FarRoot.Coordinate.Y), FarRoot.Level}, MakeArrayView(&FarRoot, 1));
	ScopedManager.bActiveCoverageFull = false;
	ScopedManager.ActiveDirtyCoverageRects = {{FIntPoint::ZeroValue, FIntPoint(16, 16)}};
	ScopedManager.ResolveTransitionVisibility();
	ScopedManager.Publisher->Tick();
	TestTrue(TEXT("Prepared children retain the coarse parent until their Native Atom receipts"), ScopedManager.VisibleTerrainNodes.Contains(NearRoot));
	for (const auto& Child : NearChildren)
		TestFalse(TEXT("A dirty root cannot publish a prepared child without a Native Atom receipt"), ScopedManager.VisibleTerrainNodes.Contains(Child));
	TestTrue(TEXT("A distant unready root retains its previous committed ownership"), ScopedManager.VisibleTerrainNodes.Contains(FarRoot));
	UWorld* FixtureWorld = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(FixtureWorld);
	const FVoxelSurfaceTileKey SurfaceKey{FIntPoint::ZeroValue, 2};
	auto Surface = MakeShared<FVoxelSurfaceTileData, ESPMode::ThreadSafe>();
	Surface->Key = SurfaceKey;
	Surface->Geometry = {{EVoxelTerrainRepresentation::Surface, FIntVector(SurfaceKey.Coordinate.X, SurfaceKey.Coordinate.Y, 0), SurfaceKey.Level}, 2, -128, 256, 17, 23};
	Surface->Side = 33;
	Surface->Step = 2;
	Surface->Revision = 1;
	Surface->GroundZ.Init(32, Surface->GetVertexCount());
	Surface->WaterZ.Init(MIN_int32, Surface->GetVertexCount());
	Surface->SurfaceMaterial.Init(0, Surface->GetVertexCount());
	Surface->Flags.Init(0, Surface->GetVertexCount());
	ScopedManager.SurfaceData.Add(SurfaceKey, Surface);
	ScopedManager.SurfaceRevisions.Add(SurfaceKey, 1);
	ScopedManager.bHeightfieldTopologyDirty = true;
	ScopedManager.SurfaceReady.Add(SurfaceKey);
	ScopedManager.GetInputRoot().SurfaceWanted.Add(SurfaceKey);
	ScopedManager.SurfaceSources.Add(SurfaceKey, FVoxelViewSourceId::NewId());
	Interest.Geometry.Add(Surface->Geometry.Key, Surface->Geometry);
	ScopedManager.IndexCoverage(SurfaceKey);
	ScopedManager.VisibleTerrainNodes.Reset();
	ScopedManager.VisibleTerrainNodesByGroup.Reset();
	ScopedManager.CommitTerrainOwnership({1, FIntPoint(NearRoot.Coordinate.X, NearRoot.Coordinate.Y), NearRoot.Level}, MakeArrayView(&NearRoot, 1));
	ScopedManager.CommitTerrainOwnership({1, FIntPoint(FarRoot.Coordinate.X, FarRoot.Coordinate.Y), FarRoot.Level}, MakeArrayView(&FarRoot, 1));
	ScopedManager.bActiveCoverageFull = false;
	ScopedManager.ActiveDirtyCoverageRects = {{FIntPoint::ZeroValue, FIntPoint(16, 16)}};
	ScopedManager.ResolveTransitionVisibility();
	ScopedManager.Publisher->Tick();
	TestTrue(TEXT("Fixture has a pending heightfield topology plan"), !ScopedManager.PendingHeightfieldPlanOwners.IsEmpty());
	for (const auto& Child : NearChildren)
		TestFalse(TEXT("A pending heightfield plan cannot substitute for a Native Fine receipt"), ScopedManager.VisibleTerrainNodes.Contains(Child));
	const FVoxelHeightfieldNodeKey SurfaceOwner{EVoxelHeightfieldRepresentation::Surface, SurfaceKey.Coordinate, SurfaceKey.Level};
	const auto* FirstPlan = ScopedManager.HeightfieldCoveragePlans.Find(SurfaceOwner);
	if (!TestNotNull(TEXT("The actual Surface owner has an immutable local coverage plan"), FirstPlan))
	{
		AddInfo(FString::Printf(TEXT("Surface: level=%d ready=%d wanted=%d actor=%d plans=%d pendingOwners=%d full=%d scopes=%d"),
			SurfaceKey.Level, ScopedManager.SurfaceReady.Contains(SurfaceKey), ScopedManager.GetInputRoot().SurfaceWanted.Contains(SurfaceKey),
			ScopedManager.SurfaceSources.FindRef(SurfaceKey).IsValid(), ScopedManager.HeightfieldCoveragePlans.Num(),
			ScopedManager.PendingHeightfieldPlanOwners.Num(), ScopedManager.bActiveCoverageFull, ScopedManager.ActiveDirtyCoverageRects.Num()));
		for (const auto& Pair : ScopedManager.HeightfieldCoveragePlans)
			AddInfo(FString::Printf(TEXT("Plan: rep=%d coordinate=%s level=%d"), int32(Pair.Key.Representation), *Pair.Key.Coordinate.ToString(), Pair.Key.Level));
		ScopedScheduler.StopAndJoin();
		return false;
	}
	const uint64 FirstPending = FirstPlan->PendingSignature;
	auto RevisedSurface = MakeShared<FVoxelSurfaceTileData, ESPMode::ThreadSafe>(*Surface);
	RevisedSurface->Revision = 2;
	ScopedManager.SurfaceData.Add(SurfaceKey, RevisedSurface);
	ScopedManager.SurfaceRevisions.Add(SurfaceKey, 2);
	ScopedManager.IndexCoverage(SurfaceKey);
	ScopedManager.RebuildHeightfieldTransitions({});
	const auto& PendingState = ScopedManager.HeightfieldCoveragePlans.FindChecked(SurfaceOwner);
	TestTrue(TEXT("Heightfield changes coalesce without canceling the in-flight immutable plan"),
		FirstPending != 0 && PendingState.PendingSignature == FirstPending && PendingState.DesiredSignature != FirstPending);
	const double HeightfieldDeadline = FPlatformTime::Seconds() + 10.0;
	while (ScopedScheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < HeightfieldDeadline)
	{
		ScopedScheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("A changed local input rejects the completed stale heightfield proof"),
		!ScopedManager.HeightfieldCoveragePlans.FindChecked(SurfaceOwner).Plan.IsValid() &&
		ScopedManager.HeightfieldCoveragePlans.FindChecked(SurfaceOwner).PendingSignature == 0);
	ScopedManager.RebuildHeightfieldTransitions({});
	while (ScopedScheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < HeightfieldDeadline)
	{
		ScopedScheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	ScopedManager.RebuildHeightfieldTransitions({});
	TestTrue(TEXT("The coalesced heightfield revision eventually releases its topology wait"), ScopedManager.PendingHeightfieldPlanOwners.IsEmpty());
	ScopedScheduler.StopAndJoin();
	ScopedManager.Reset();
	FixtureWorld->DestroyWorld(false);
	GEngine->DestroyWorldContext(FixtureWorld);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelHeightfieldLocalDependenciesTest,
	"WHFramework.Voxel.Rendering.HeightfieldLocalDependencies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelHeightfieldLocalDependenciesTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TArray<int32> Ground, Water;
	TArray<uint16> Materials;
	TArray<uint8> WaterKinds;
	WaterKinds.Init(0, 33 * 33);
	Ground.Init(32, 33 * 33);
	Water.Init(MIN_int32, 33 * 33);
	Materials.Init(0, 33 * 33);
	TArray<FVoxelHeightfieldTileView> Views;
	const auto AddView = [&](FIntPoint Coordinate, FIntPoint Origin, int32 Step)
	{
		auto& View = Views.AddDefaulted_GetRef();
		View.Key = {EVoxelHeightfieldRepresentation::Surface, Coordinate, static_cast<uint8>(Step == 2 ? 1 : 0)};
		View.Origin = Origin;
		View.Side = 33;
		View.Step = Step;
		View.Revision = 1;
		View.Ground = Ground;
		View.Water = Water;
		View.WaterKind = WaterKinds;
		View.Material = Materials;
	};
	AddView({-1, 0}, {-32, 0}, 1);
	AddView({0, 0}, {0, 0}, 2);
	AddView({-1, 1}, {-32, 32}, 1);
	AddView({1000, 0}, {32000, 0}, 1);
	const FVoxelHeightfieldNodeKey PendingKey{EVoxelHeightfieldRepresentation::Surface, {2, 0}, 0};
	const TArray<FVoxelHeightfieldTileFootprint> Pending{{PendingKey, {64, 0}, 32, 1}};
	TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldNodeKey>> Dependencies;
	FVoxelHeightfieldTransitionBuilder::GatherOwnerDependencies(Views, Pending, Dependencies);
	TestTrue(TEXT("A coarse owner retains its adjacent pending tile"), Dependencies[Views[1].Key].Contains(PendingKey));
	TestEqual(TEXT("Distant owner remains independent of local waits"), Dependencies[Views[3].Key].Num(), 1);
	TArray<FVoxelHeightfieldTransitionEdge> Edges;
	FString Error;
	if (!TestTrue(TEXT("The full heightfield transition reference builds"),
		FVoxelHeightfieldTransitionBuilder::BuildEdges(Views, Edges, Error))) return false;
	TestTrue(TEXT("Fixture contains mixed LOD seams"), !Edges.IsEmpty());
	for (const auto& Edge : Edges)
	{
		TestTrue(TEXT("Local dependency closure includes every full-plan seam"), Dependencies[Edge.Owner].Contains(Edge.Neighbor));
	}
	TMap<FVoxelHeightfieldNodeKey, TArray<FVoxelHeightfieldNodeKey>> Reordered;
	Algo::Reverse(Views);
	FVoxelHeightfieldTransitionBuilder::GatherOwnerDependencies(Views, Pending, Reordered);
	for (const auto& Pair : Dependencies)
	{
		TestTrue(TEXT("Dependency order is independent of input loading order"), Reordered[Pair.Key] == Pair.Value);
	}
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelInterestInputBudgetTest,
	"WHFramework.Voxel.Rendering.InterestInputBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelInterestInputBudgetTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Interest budget fixture has a World"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	AMainModule* Owner = World->SpawnActor<AMainModule>();
	if (!TestNotNull(TEXT("Interest budget fixture has its module owner"), Owner)) return false;
	UVoxelModule* Module = NewObject<UVoxelModule>(Owner);
	if (!TestEqual(TEXT("Fine admissions resolve the actual authority World"), Module->GetWorld(), World)) return false;
	Module->Epoch = 9;
	FString FixtureError;
	if (!Module->Registry.Build({}, false, FixtureError)) return false;
	const auto Shapes = MakeShared<FVoxelShapeRegistry, ESPMode::ThreadSafe>();
	Shapes->BuildDefaults();
	Module->Shapes = Shapes;
	Module->Generator = VoxelTest::MakeGenerator();
	const auto Control = FVoxelGenerationConsumerKey::Section(9, FGuid::NewGuid(), 1, {}, 1);
	Module->GenerationConfig = VoxelTest::MakeGenerationConfig();
	Module->Manifest.WorldId = FGuid::NewGuid();
	Module->Manifest.RegistryHash = Module->GetRegistry()->Hash;
	Module->Manifest.RecipeHash = Module->GenerationConfig->Recipe->RecipeHash;
	Module->Manifest.Settings = Module->GenerationConfig->Recipe->Settings;
	Module->Manifest.BaseSampleHash = VoxelBinary::Mix64(FVoxelManifestCodec::RecipeFingerprint(Module->Manifest) ^
		VoxelBinary::Mix64(Module->Manifest.RegistryHash) ^ VoxelBinary::Mix64(Module->Manifest.RecipeHash));
	if (!TestTrue(TEXT("Fine admission fixture has its actual generation identity"), Module->Manifest.BaseSampleHash != 0)) return false;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> GeometryResources, FactResources;
	if (!TestTrue(TEXT("Fine budget fixture admits original geometry and fact ledgers"),
		FVoxelTerrainResources::Create({}, Control, GeometryResources, FixtureError) &&
		FVoxelTerrainResources::Create({}, Control, FactResources, FixtureError) &&
		FVoxelTaskScheduler::Create(GeometryResources.ToSharedRef(), FactResources.ToSharedRef(), Control, Module->Scheduler, FixtureError))) return false;
	ON_SCOPE_EXIT
	{
		Module->Scheduler->StopAndJoin();
		Module->Runtime.Reset();
		Module->Scheduler.Reset();
	};
	auto& Scheduler = *Module->Scheduler;
	if (!TestTrue(TEXT("Fine budget fixture initializes its original scheduler epoch"), Scheduler.InitializeStatistics(9))) return false;
	Module->Runtime = MakeUnique<FVoxelWorldRuntime>(9, true, Module->Registry.GetSnapshot().ToSharedRef(),
		Module->Generator.ToSharedRef(), FactResources.ToSharedRef());
	Scheduler.BindFrameContext(Module->GetTerrainFrameContext());
	FVoxelViewManager Manager(*Module, Scheduler, 9);
	uint64 FrameSerial = 0;
	TSharedPtr<const FVoxelInterestSet, ESPMode::ThreadSafe> QueuedDesired;
	const auto FlushDesired = [&]
	{
		if (QueuedDesired && Module->DesiredInterest == Module->CurrentInterest)
			Module->DesiredInterest = MoveTemp(QueuedDesired);
	};
	const auto QueueDesired = [&](FVoxelInterestSet Value)
	{
		Value.SafetyWorldEpoch = Module->Epoch;
		Value.SafetyRegistryRevision = Module->GetRegistry()->Hash;
		Value.SafetyShapeRevision = Module->Shapes->GetContentHash();
		Value.SafetyVersions = MakeShared<const FVoxelChangeHierarchySnapshot, ESPMode::ThreadSafe>(Module->Runtime->GetChangeHierarchy().CaptureVersionSnapshot());
		for (const auto& Pair : Value.Sources)
		{
			auto& Registered = Module->Sources.FindOrAdd(Pair.Key);
			Registered.Owner = Module;
			Registered.Value = Pair.Value.Source;
		}
		const auto Snapshot = MakeShared<const FVoxelInterestSet, ESPMode::ThreadSafe>(MoveTemp(Value));
		if (Module->DesiredInterest == Module->CurrentInterest) Module->DesiredInterest = Snapshot;
		else QueuedDesired = Snapshot;
	};
	const auto BeginFrame = [&]
	{
		return Module->GetTerrainFrameContext().Begin(9, ++FrameSerial, FPlatformTime::Seconds(), {});
	};
	const auto EndFrame = [&]
	{
		Module->GetTerrainFrameContext().End(FPlatformTime::Seconds());
	};
	const auto TickManager = [&](const uint64 Revision, TConstArrayView<FVector> Observers)
	{
		verify(BeginFrame());
		Manager.Tick(Revision, Observers);
		EndFrame();
		FlushDesired();
	};
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	TickManager(0, {});
	TestFalse(TEXT("Bootstrap waits for the first accepted publication snapshot"), Manager.PendingInterestSnapshot.IsValid());
	TestFalse(TEXT("Unaccepted bootstrap state is never applied as a complete interest"), Manager.AppliedInterestSnapshot.IsValid());
	FVoxelInterestSet EmptyInterest;
	EmptyInterest.PublicationRevision = 1;
	Module->CurrentInterest = MakeShared<const FVoxelInterestSet, ESPMode::ThreadSafe>(EmptyInterest);
	Module->DesiredInterest = Module->CurrentInterest;
	Manager.AppliedInterestSnapshot = Module->CurrentInterest;
	Manager.AppliedInterestRevision = 1;
	Manager.QueueRetirement({4, FIntVector(9999), 0});
	int32 Committed = 0;
	const FGuid EmptyGroup = Manager.Publisher->BeginGroup({0, FIntVector(9999), 0});
	Manager.Publisher->SetOwnershipChanged(EmptyGroup, true);
	Manager.Publisher->SetCommitCallbacks({},
		[&Committed](TConstArrayView<FVoxelPublishGroupKey> Keys)
		{
			Committed += Keys.Num();
		});
	TestTrue(TEXT("An actorless ownership group is accepted"), Manager.Publisher->CommitGroup(EmptyGroup));
	const auto HoldCoverage = []() -> FVoxelGameThreadContinuation
	{
		while (true)
		{
			co_await FVoxelGameThreadContinuation::Yield();
		}
	};
	Manager.CoverageContinuation = HoldCoverage();
	for (int32 Frame = 0; Frame < 32; ++Frame)
	{
		TickManager(1, {});
	}
	TestTrue(TEXT("Immutable publication preparation progresses while a coverage snapshot is pinned"), Manager.Publisher->HasActiveGroups());
	TestTrue(TEXT("A pinned coverage snapshot retains ready commits for its next boundary"), Manager.Publisher->HasReadyCommits());
	TestEqual(TEXT("Coverage keeps visibility and ownership commits frozen"), Committed, 0);
	Manager.CoverageContinuation.Reset();
	for (int32 Frame = 0; Frame < 2048 && Committed == 0; ++Frame)
	{
		Manager.MarkCoverageDirty();
		TickManager(1, {});
	}
	TestEqual(TEXT("Publication progresses across continuous coverage and retirement windows"), Committed, 1);
	Manager.Reset();
	Manager.AppliedInterestSnapshot = Module->CurrentInterest;
	Manager.AppliedInterestRevision = 1;
	Manager.bActiveCoverageFull = false;
	Manager.ActiveDirtyCoverageRects.Add({FIntPoint::ZeroValue, FIntPoint(16)});
	Manager.CoverageContinuation = HoldCoverage();
	const FIntVector RetiredKey(9999);
	const FVoxelPublishGroupKey Ticket{4, RetiredKey, 0};
	Manager.FineReady.Add(RetiredKey);
	Manager.FineRevisions.Add(RetiredKey, 1);
	Manager.FineLastWanted.Add(RetiredKey, FPlatformTime::Seconds() - 1.0);
	Manager.IndexCoverage(FVoxelViewKey{RetiredKey, 0});
	Manager.QueueRetirement(Ticket);
	for (int32 Frame = 0; Frame < 2048 && Manager.RetirementIndices.Contains(Ticket); ++Frame)
	{
		verify(BeginFrame());
		Manager.CleanupRetiredRepresentations(FPlatformTime::Seconds());
		EndFrame();
	}
	TestFalse(TEXT("局部覆盖存活时无关区域退休仍可完成"), Manager.RetirementIndices.Contains(Ticket));
	TestFalse(TEXT("退休释放已无实际 Owner 的旧 Fine 准备状态"), Manager.FineReady.Contains(RetiredKey));
	TestTrue(TEXT("退休不消费另一个覆盖续行"), Manager.CoverageContinuation.IsValid());
	TestTrue(TEXT("退休 Yield 后恢复原覆盖范围"), Manager.ActiveDirtyCoverageRects.Num() == 1 && Manager.ActiveDirtyCoverageRects[0].Min == FIntPoint::ZeroValue);
	Manager.QueueRetirement(Ticket);
	Manager.FineReady.Add(RetiredKey);
	Manager.RetirementCandidate = Ticket;
	Manager.RetirementBounds = Manager.FineCoverageRect(RetiredKey);
	Manager.RetirementCoverageRoot = Manager.CoverageIndex;
	Manager.RetirementHeightfieldRoot = Manager.HeightfieldCoverageIndex;
	Manager.RetirementContinuation = HoldCoverage();
	Manager.GetInputRoot().FineWanted.Add(RetiredKey);
	verify(BeginFrame());
	Manager.CleanupRetiredRepresentations(FPlatformTime::Seconds());
	EndFrame();
	TestFalse(TEXT("重新被需要的候选撤销旧退休续行"), Manager.RetirementContinuation.IsValid());
	TestFalse(TEXT("需求恢复撤销旧退休登记"), Manager.RetirementIndices.Contains(Ticket));
	TestTrue(TEXT("需求恢复保留原 Fine 准备状态"), Manager.FineReady.Contains(RetiredKey));
	Manager.GetInputRoot().FineWanted.Add(FIntVector::ZeroValue);
	Manager.IndexCoverage(FVoxelViewKey{FIntVector::ZeroValue, 0});
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(9, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	const auto CapacityBefore = Resources->Snapshot();
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
		CapacityBefore.Limits.WorkingBytes - CapacityBefore.Used[int32(EVoxelTerrainResourcePool::Working)] - 1, Blocker)) return false;
	FVoxelViewManager::FCoveragePassSnapshot BlockedSnapshot;
	auto Capture = Manager.BuildCoverageSnapshotSteps(BlockedSnapshot);
	for (int32 Step = 0; Step < 16384 && !Capture.IsComplete(); ++Step) Capture.Resume();
	TestTrue(TEXT("覆盖容量不足时结束原捕获并释放门禁"), Capture.IsComplete());
	TestFalse(TEXT("容量等待不能冒充完整空覆盖"), BlockedSnapshot.bComplete);
	Capture.Reset();
	Manager.CoverageContinuation.Reset();
	Manager.bActiveCoverageFull = true;
	const FVoxelPublishGroupKey RetryKey{2, FIntVector::ZeroValue, 0};
	const FGuid RetryGroup = Manager.BeginPublishGroup(RetryKey);
	Manager.ActiveCoveragePublishGroups = MoveTemp(Manager.OpenPublishGroups);
	Manager.CoverageContinuation = Manager.ResolveTransitionVisibilitySteps();
	{
		TGuardValue<bool> ReadScope(Manager.bCoverageReadScope, true);
		for (int32 Step = 0; Step < 16384 && !Manager.CoverageContinuation.IsComplete(); ++Step)
			Manager.CoverageContinuation.Resume();
	}
	TestTrue(TEXT("真实覆盖流程在 Working 容量不足时结束并登记重试"),
		Manager.CoverageContinuation.IsComplete() && Manager.bCoverageRetryRequested && Manager.bCoverageDirty);
	TestTrue(TEXT("未提交组原样归还，不被下一轮全量覆盖覆盖掉"),
		Manager.ActiveCoveragePublishGroups.IsEmpty() && Manager.OpenPublishGroups.FindRef(RetryKey) == RetryGroup);
	Manager.CoverageContinuation.Reset();
	TestFalse(TEXT("失败捕获退出后，同区域的结果应用与发布门禁已释放"),
		Manager.ShouldDeferCoverage(Manager.FineCoverageRect(FIntVector::ZeroValue)));
	Blocker.Reset();
	Capture = Manager.BuildCoverageSnapshotSteps(BlockedSnapshot);
	for (int32 Step = 0; Step < 16384 && !Capture.IsComplete(); ++Step) Capture.Resume();
	TestTrue(TEXT("容量释放后重新捕获完整覆盖"), Capture.IsComplete() && BlockedSnapshot.bComplete);
	Capture.Reset();
	BlockedSnapshot = {};
	TestEqual(TEXT("捕获退休后 Working 容量归还"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], uint64(0));
	Manager.Reset();
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual;
	Source.View.FineRadiusCells = 128;
	Source.View.FineVerticalHalfExtentCells = 32;
	Source.View.InitialChunkSideCells = 16;
	FVoxelInterestManager Planner;
	FVoxelInterestSet Interest = Planner.Compute(MakeArrayView(&Source, 1), Module->GetManifest(), Module->GetViewSettings());
	Interest.PublicationRevision = 1;
	QueueDesired(Interest);
	if (!TestTrue(TEXT("Budget fixture has a valid prepared interest"), Interest.PlanningError.IsEmpty() && Interest.PublicationInputs.IsValid())) return false;
	const auto Advance = [&](uint64 Revision)
	{
		verify(BeginFrame());
		Manager.bTerrainFrameBudgetActive = true;
		Manager.AdvanceInterestInputs(Revision, {});
		Manager.bTerrainFrameBudgetActive = false;
		EndFrame();
		FlushDesired();
	};
	// 原生容器扩容留下的旧耗时尖峰必须在低预算新帧恢复，仍使用完整发布游标。
	Manager.InterestMicrostepEstimateMilliseconds = 8.0;
	Advance(1);
	TestEqual(TEXT("A large interest is not applied in one publication slice"), Manager.AppliedInterestRevision, uint64(0));
	TestTrue(TEXT("The first slice does not copy the complete Fine domain"), Manager.GetInputRoot().FineWanted.Num() < Interest.FineSections->Num());
	Source.View.InitialChunkSideCells = 32;
	Source.Center.X = 128;
	Interest = Planner.Compute(MakeArrayView(&Source, 1), Module->GetManifest(), Module->GetViewSettings());
	Interest.PublicationRevision = 2;
	QueueDesired(Interest);
	if (!TestTrue(TEXT("Superseding 32-cell interest remains valid"), Interest.PlanningError.IsEmpty())) return false;
	Manager.InterestMicrostepEstimateMilliseconds = 8.0;
	for (int32 Slice = 0; Slice < 4096 && Manager.AppliedInterestRevision != 2; ++Slice) Advance(2);
	TestEqual(TEXT("A superseded cursor converges to the latest interest"), Manager.AppliedInterestRevision, uint64(2));
	TestTrue(TEXT("No Fine demand from the superseded interest remains"),
		Manager.GetInputRoot().FineWanted.Num() == Interest.FineSections->Num() && Manager.GetInputRoot().FineWanted.Difference(*Interest.FineSections).IsEmpty());
	TestTrue(TEXT("The complete 32-cell Page partition is accepted together"),
		Manager.GetInputRoot().FinePagePlan.Pages.Num() == Interest.FinePagePlan->Pages.Num() &&
		Manager.GetInputRoot().FinePagePlan.SectionOwners.Num() == Interest.FinePagePlan->SectionOwners.Num());
	TestTrue(TEXT("Applied interest retains runnable admission priorities"), !Manager.AdmissionPriorities.IsEmpty());
	TestEqual(TEXT("Applied admission entries match the accepted worker snapshot"), Manager.Admissions.Num(), Interest.Admissions.Num());
	TestEqual(TEXT("The final complete snapshot was adopted"), Manager.AppliedInterestSnapshot->PublicationRevision, uint64(2));
	for (int32 Slice = 0; Slice < 4096 && Manager.CommittedWorkspaceEvents; ++Slice) Advance(2);
	if (!TestFalse(TEXT("Accepted input events finish before the next workspace begins"), Manager.CommittedWorkspaceEvents.IsValid())) return false;
	Interest.PublicationRevision = 3;
	QueueDesired(Interest);
	if (!TestTrue(TEXT("Begin the production workspace before advancing its bounded cursor"),
		Manager.BeginInterestWorkspace(Module->GetDesiredInterestSnapshot()))) return false;
	if (!TestTrue(TEXT("A complete in-flight snapshot is retained"), Manager.PendingInterestSnapshot.IsValid())) return false;
	bool bThirdSnapshotCompleted = false;
	for (uint64 Revision = 4; Revision <= 8; ++Revision)
	{
		Source.Center.X += 16;
		Interest = Planner.Compute(MakeArrayView(&Source, 1), Module->GetManifest(), Module->GetViewSettings());
		Interest.PublicationRevision = Revision;
		QueueDesired(Interest);
		Advance(Revision);
		bThirdSnapshotCompleted |= Manager.AppliedInterestRevision == 3;
		if (!bThirdSnapshotCompleted)
		{
			if (!TestTrue(TEXT("Moving layouts retain the unfinished original snapshot"), Manager.PendingInterestSnapshot.IsValid())) return false;
			TestEqual(TEXT("Moving layouts cannot restart the in-flight input"), Manager.PendingInterestSnapshot->PublicationRevision, uint64(3));
		}
	}
	for (int32 Slice = 0; Slice < 8192 && Manager.AppliedInterestRevision != 8; ++Slice)
	{
		Advance(8);
		bThirdSnapshotCompleted |= Manager.AppliedInterestRevision == 3;
	}
	TestTrue(TEXT("The original moving snapshot completes before its queued successors"), bThirdSnapshotCompleted);
	TestEqual(TEXT("The latest of several moving layouts is eventually adopted"), Manager.AppliedInterestRevision, uint64(8));
	TestTrue(TEXT("Latest layout owns the complete Fine partition"),
		Manager.GetInputRoot().FineWanted.Num() == Interest.FineSections->Num() && Manager.GetInputRoot().FineWanted.Difference(*Interest.FineSections).IsEmpty());
	for (const auto& Pair : Interest.Exact)
	{
		FVoxelSection* Section = Module->Runtime->FindOrAllocate(Pair.Key, 1);
		TArray<FVoxelBlockState> Blocks;
		Blocks.SetNum(4096);
		if (!Section || !Module->Runtime->PublishBase(Pair.Key, Section->Stamp, MoveTemp(Blocks), FixtureError) ||
			!Module->Runtime->PublishFinal(Pair.Key, 0, {}, {}, FixtureError)) return false;
	}
	Manager.Publisher->BeginGroup({0, FIntVector(9999), 0});
	Manager.bCoverageDirty = false;
	for (int32 Frame = 0; Frame < 8 && Scheduler.ActiveCount() == 0 && Manager.FineReady.IsEmpty(); ++Frame)
		TickManager(8, {});
	TestTrue(TEXT("An uncommitted publication cannot consume every shared operation and starve actual Fine mesh admission"),
		Scheduler.ActiveCount() > 0 || !Manager.FineReady.IsEmpty());
	const FIntVector RebuildKey = Source.Center / 16;
	if (!TestTrue(TEXT("Neighbor rebuild uses an interior demanded F0 section"), Interest.FineSections->Contains(RebuildKey))) return false;
	FVoxelSection* RebuildSection = Module->Runtime->FindSection(RebuildKey);
	Manager.Reset();
	for (int32 Slice = 0; Slice < 4096 && Manager.AppliedInterestRevision != 8; ++Slice) Advance(8);
	if (!TestEqual(TEXT("Reset restores the complete admission snapshot"), Manager.AppliedInterestRevision, uint64(8))) return false;
	Manager.FineReady.Add(RebuildKey);
	Manager.FineRevisions.Add(RebuildKey, RebuildSection->CommittedRevision);
	Manager.InvalidateNeighbors(RebuildKey - FIntVector(1, 0, 0));
	TestFalse(TEXT("Neighbor changes revoke Fine readiness"), Manager.FineReady.Contains(RebuildKey));
	FVoxelViewAdmission RebuildAdmission;
	RebuildAdmission.Kind = EVoxelViewAdmissionKind::Fine;
	RebuildAdmission.FineKey = RebuildKey;
	const FVoxelFineShardKey RebuildShard{RebuildKey * 16, 16};
	for (int32 Step = 0; Step < 16; ++Step)
	{
		Manager.TrySubmitAdmission(RebuildAdmission);
		const auto* Slot = Manager.FineShardSlots.Find(RebuildShard);
		if (Slot && Slot->bSubmitted) break;
	}
	const auto* RebuiltSlot = Manager.FineShardSlots.Find(RebuildShard);
	TestTrue(TEXT("Unchanged section revision still rebuilds invalidated neighbor geometry"),
		RebuiltSlot && RebuiltSlot->bSubmitted && Scheduler.Has(RebuiltSlot->SubmittedStamp, EVoxelTaskKind::BuildFineShard));
	Manager.Reset();
	for (int32 Slice = 0; Slice < 4096 && Manager.AppliedInterestRevision != 8; ++Slice) Advance(8);
	if (!TestEqual(TEXT("Retirement fixture restores complete admission state"), Manager.AppliedInterestRevision, uint64(8))) return false;
	Interest.PublicationRevision = 9;
	QueueDesired(Interest);
	Advance(9);
	Manager.QueueRetirement({4, FIntVector(9999), 0});
	for (int32 Frame = 0; Frame < 1024 && Manager.AppliedInterestRevision != 9; ++Frame)
	{
		TickManager(9, {});
	}
	TestEqual(
		TEXT("Retirement cannot start a coverage continuation while publication inputs are partially applied"), Manager.AppliedInterestRevision, uint64(9));
	Scheduler.StopAndJoin();
	Manager.Reset();
	return true;
}

#endif
