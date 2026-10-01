#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Map/VoxelMapTileCache.h"
#include "HAL/PlatformProcess.h"
#include "Engine/Texture2D.h"
#include "EngineGlobals.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Streaming/VoxelInterestRuntime.h"
#include "Voxel/Streaming/VoxelResidencyManager.h"
#include "Voxel/Rendering/VoxelViewManager.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Tests/VoxelTestUtilities.h"

namespace
{
	template<typename KeyType>
	bool SameSet(const TSet<KeyType>& InA, const TSet<KeyType>& InB)
	{
		if (InA.Num() != InB.Num()) return false;
		for (const KeyType& Key : InA)
		{
			if (!InB.Contains(Key)) return false;
		}
		return true;
	}

	template<typename KeyType>
	int32 CountTwoToOneViolations(const TSet<KeyType>& InTiles,
		const int32 InBaseSide, int32& OutOverlaps, int32& OutDifferentLevels)
	{
		TArray<KeyType> Tiles;
		for (const KeyType& Tile : InTiles) Tiles.Add(Tile);
		int32 Violations = 0;
		OutOverlaps = 0;
		OutDifferentLevels = 0;
		for (int32 A = 0; A < Tiles.Num(); ++A)
		{
			const int64 SideA = static_cast<int64>(InBaseSide) << Tiles[A].Level;
			const int64 MinAX = static_cast<int64>(Tiles[A].Coordinate.X) * SideA;
			const int64 MinAY = static_cast<int64>(Tiles[A].Coordinate.Y) * SideA;
			for (int32 B = A + 1; B < Tiles.Num(); ++B)
			{
				const int64 SideB = static_cast<int64>(InBaseSide) << Tiles[B].Level;
				const int64 MinBX = static_cast<int64>(Tiles[B].Coordinate.X) * SideB;
				const int64 MinBY = static_cast<int64>(Tiles[B].Coordinate.Y) * SideB;
				const int64 XOverlap = FMath::Min(MinAX + SideA, MinBX + SideB) - FMath::Max(MinAX, MinBX);
				const int64 YOverlap = FMath::Min(MinAY + SideA, MinBY + SideB) - FMath::Max(MinAY, MinBY);
				if (XOverlap > 0 && YOverlap > 0) ++OutOverlaps;
				const bool bShareEdge = (XOverlap == 0 && YOverlap > 0) ||
					(YOverlap == 0 && XOverlap > 0);
				if (bShareEdge && Tiles[A].Level != Tiles[B].Level) ++OutDifferentLevels;
				if (bShareEdge && FMath::Abs(static_cast<int32>(Tiles[A].Level) -
					static_cast<int32>(Tiles[B].Level)) > 1) ++Violations;
			}
		}
		return Violations;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelSurfaceTwoToOneBalanceTest,
	"WHFramework.Voxel.Streaming.SurfaceTwoToOneBalance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSurfaceTwoToOneBalanceTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelViewSettings Settings;
	FVoxelStreamingSource Sources[2];
	for (FVoxelStreamingSource& Source : Sources)
	{
		Source.Capabilities = EVoxelStreamingCapability::Data |
			EVoxelStreamingCapability::FineVisual |
			EVoxelStreamingCapability::WorldVisual;
		Source.View.FineRadiusCells = 0;
		Source.View.VoxelProxyRadiusCells = 64;
		Source.View.SurfaceRadiusCells = 8192;
		Settings.MacroRadiusCells = 16384;
		Source.View.MaximumSurfaceLevel = 5;
		Settings.MaximumSurfaceTiles = 512;
	}
	Sources[0].Center = FIntVector::ZeroValue;
	Sources[1].Center = FIntVector(3072, 1024, 0);
	const FVoxelInterestManager Manager;
	const FVoxelInterestSet Interest = Manager.Compute(MakeArrayView(Sources), Manifest, Settings);
	int32 Overlaps = 0;
	int32 DifferentLevels = 0;
	const int32 Violations = CountTwoToOneViolations(Interest.Surface,
		Settings.SurfaceTileSide, Overlaps, DifferentLevels);
	TestTrue(TEXT("Surface plan contains neighboring LOD levels"), DifferentLevels > 0);
	TestEqual(TEXT("Surface tiles do not overlap after source merge"), Overlaps, 0);
	TestEqual(TEXT("Every shared surface edge is at most 2:1"), Violations, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSimulationAnchorBoundaryTest,
	"WHFramework.Voxel.Streaming.SimulationAnchorBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSimulationAnchorBoundaryTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::Simulation;
	Source.Purpose = EVoxelStreamingSourcePurpose::SimulationAnchor;
	Source.ExactRadius = Source.VerticalExactRadius = 16;
	Source.SimulationRadius = 1;
	for (const int32 X : {0, -16})
	{
		Source.Center = FIntVector(X, 0, 0);
		const auto Interest = FVoxelInterestManager().Compute(MakeArrayView(&Source, 1), Manifest, {});
		const auto* Demand = Interest.Exact.Find(FIntVector(X / 16, 0, 0));
		TestTrue(TEXT("A small simulation source retains the containing section at a cell boundary"), Demand && Demand->bSimulation);
		TestTrue(TEXT("Simulation creates no visual demand"), Interest.VoxelProxy.IsEmpty() && Interest.Surface.IsEmpty() && Interest.Macro.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelMacroTwoToOneBalanceTest,
	"WHFramework.Voxel.Streaming.MacroTwoToOneBalance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMacroTwoToOneBalanceTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelViewSettings Settings;
	FVoxelStreamingSource Sources[2];
	for (FVoxelStreamingSource& Source : Sources)
	{
		Source.Capabilities = EVoxelStreamingCapability::Data |
			EVoxelStreamingCapability::FineVisual |
			EVoxelStreamingCapability::WorldVisual;
		Source.View.FineRadiusCells = 0;
		Source.View.VoxelProxyRadiusCells = 64;
		Source.View.SurfaceRadiusCells = 2048;
		Settings.MacroRadiusCells = 96000;
		Settings.MaximumMacroTiles = 512;
	}
	Sources[0].Center = FIntVector::ZeroValue;
	Sources[1].Center = FIntVector(16384, 4096, 0);
	const FVoxelInterestManager Manager;
	const FVoxelInterestSet Interest = Manager.Compute(MakeArrayView(Sources), Manifest, Settings);
	int32 Overlaps = 0;
	int32 DifferentLevels = 0;
	const int32 Violations = CountTwoToOneViolations(Interest.Macro,
		Settings.MacroTileSide, Overlaps, DifferentLevels);
	TestTrue(TEXT("Macro plan contains neighboring LOD levels"), DifferentLevels > 0);
	TestEqual(TEXT("Macro tiles do not overlap after source merge"), Overlaps, 0);
	TestEqual(TEXT("Every shared macro edge is at most 2:1"), Violations, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelInterestLayerAnchorTest,
	"WHFramework.Voxel.Streaming.Interest.LayerAnchors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelInterestLayerAnchorTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelViewSettings Settings;
	Settings.MacroRadiusCells = 8192;
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Capabilities = EVoxelStreamingCapability::Data |
		EVoxelStreamingCapability::Collision |
		EVoxelStreamingCapability::FineVisual |
		EVoxelStreamingCapability::WorldVisual;
	Source.ExactRadius = 16;
	Source.CollisionRadius = 8;
	Source.View.FineRadiusCells = 64;
	Source.View.FinePreloadCells = 16;
	Source.View.VoxelProxyRadiusCells = 256;
	Source.View.SurfaceRadiusCells = 1024;
	FVoxelInterestRuntime Runtime;
	FVoxelInterestSet Interest;
	FVoxelInterestDelta Delta;
	bool bHasSnapshot = false;
	TestTrue(TEXT("Initial source creates interest"), Runtime.Update(
		MakeArrayView(&Source, 1), Manifest, Settings, Interest, Delta, bHasSnapshot));
	TestTrue(TEXT("Initial source publishes a snapshot"), bHasSnapshot);
	const FVoxelInterestSet Direct = FVoxelInterestManager().Compute(
		MakeArrayView(&Source, 1), Manifest, Settings);
	bool bExactKeysMatch = Interest.Exact.Num() == Direct.Exact.Num();
	for (const auto& Pair : Interest.Exact)
	{
		bExactKeysMatch &= Direct.Exact.Contains(Pair.Key);
	}
	TestTrue(TEXT("Initial exact keys match the direct planner"),
		bExactKeysMatch);
	TestTrue(TEXT("Initial proxy keys match the direct planner"),
		SameSet(Interest.VoxelProxy, Direct.VoxelProxy));
	TestTrue(TEXT("Initial surface keys match the direct planner"),
		SameSet(Interest.Surface, Direct.Surface));
	TestTrue(TEXT("Initial macro keys match the direct planner"),
		SameSet(Interest.Macro, Direct.Macro));
	FVoxelStreamingSource SubcellSource = Source;
	SubcellSource.Center.X = 1;
	TestFalse(TEXT("Movement within all anchors skips planning"),
		FVoxelInterestRuntime::NeedsUpdate(Source, SubcellSource));
	TestFalse(TEXT("Subcell movement does not change interest"), Runtime.Update(
		MakeArrayView(&SubcellSource, 1), Manifest, Settings, Interest, Delta, bHasSnapshot));
	TestFalse(TEXT("Subcell movement does not publish a snapshot"), bHasSnapshot);
	FVoxelStreamingSource NegativeSource = Source;
	NegativeSource.Center.X = -1;
	TestTrue(TEXT("Negative coordinate crossing invalidates exact anchor"),
		FVoxelInterestRuntime::NeedsUpdate(Source, NegativeSource));
	FVoxelStreamingSource RotatedSource = Source;
	RotatedSource.Direction = FVector::BackwardVector;
	TestTrue(TEXT("Large direction change refreshes scores"),
		FVoxelInterestRuntime::NeedsUpdate(Source, RotatedSource));
	const TSet<FVoxelMacroTileKey> InitialMacro = Interest.Macro;
	Source.Center.X = 4;
	Runtime.Update(MakeArrayView(&Source, 1), Manifest, Settings, Interest, Delta, bHasSnapshot);
	TestTrue(TEXT("Exact movement keeps macro tiles"), SameSet(Interest.Macro, InitialMacro));
	TestFalse(TEXT("Exact movement has no macro delta"),
		!Delta.AddedMacro.IsEmpty() || !Delta.RemovedMacro.IsEmpty());
	Source.Center.X = 20;
	Runtime.Update(MakeArrayView(&Source, 1), Manifest, Settings, Interest, Delta, bHasSnapshot);
	TestTrue(TEXT("Fine anchor movement keeps macro tiles"), SameSet(Interest.Macro, InitialMacro));
	Source.Center.X = 80;
	Runtime.Update(MakeArrayView(&Source, 1), Manifest, Settings, Interest, Delta, bHasSnapshot);
	TestTrue(TEXT("Proxy anchor movement keeps macro tiles"), SameSet(Interest.Macro, InitialMacro));
	Source.Center.X = 300;
	Runtime.Update(MakeArrayView(&Source, 1), Manifest, Settings, Interest, Delta, bHasSnapshot);
	TestTrue(TEXT("Surface anchor movement keeps macro tiles"), SameSet(Interest.Macro, InitialMacro));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelInterestMultiSourceTest,
	"WHFramework.Voxel.Streaming.Interest.MultiSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelInterestMultiSourceTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelViewSettings Settings;
	Settings.MacroRadiusCells = 4096;
	FVoxelStreamingSource Sources[2];
	Sources[0].Id = FGuid::NewGuid();
	Sources[0].Capabilities = EVoxelStreamingCapability::Data |
		EVoxelStreamingCapability::Collision |
		EVoxelStreamingCapability::FineVisual |
		EVoxelStreamingCapability::WorldVisual |
		EVoxelStreamingCapability::Readiness;
	Sources[0].Center = FIntVector(-256, -64, 0);
	Sources[0].View.FineRadiusCells = 64;
	Sources[0].View.FinePreloadCells = 16;
	Sources[0].View.VoxelProxyRadiusCells = 256;
	Sources[0].View.SurfaceRadiusCells = 512;
	Sources[0].VelocityCellsPerSecond = FVector(32.0, 0.0, 0.0);
	Sources[1] = Sources[0];
	Sources[1].Id = FGuid::NewGuid();
	Sources[1].Capabilities = EVoxelStreamingCapability::Data |
		EVoxelStreamingCapability::FineVisual |
		EVoxelStreamingCapability::LocalRefinement;
	Sources[1].Center = FIntVector(-128, -64, 0);
	Sources[1].SchedulingPriority = 1;
	Sources[1].VelocityCellsPerSecond = FVector::ZeroVector;
	FVoxelInterestRuntime Runtime;
	FVoxelInterestSet Interest;
	FVoxelInterestDelta Delta;
	bool bHasSnapshot = false;
	TestTrue(TEXT("Multiple sources create interest"), Runtime.Update(
		MakeArrayView(Sources), Manifest, Settings, Interest, Delta, bHasSnapshot));
	const FVoxelInterestSet Direct = FVoxelInterestManager().Compute(
		MakeArrayView(Sources), Manifest, Settings);
	bool bExactKeysMatch = Interest.Exact.Num() == Direct.Exact.Num();
	for (const auto& Pair : Interest.Exact)
	{
		bExactKeysMatch &= Direct.Exact.Contains(Pair.Key);
	}
	TestTrue(TEXT("Multiple sources preserve exact demand"), bExactKeysMatch);
	TestTrue(TEXT("Multiple sources preserve proxy view"),
		SameSet(Interest.VoxelProxy, Direct.VoxelProxy));
	TestTrue(TEXT("Local refinement adds no distant surface"),
		SameSet(Interest.Surface, Direct.Surface));
	TestTrue(TEXT("Local refinement adds no macro"),
		SameSet(Interest.Macro, Direct.Macro));
	int32 CriticalFineCount = 0;
	for (const auto& Pair : Interest.Exact)
	{
		if (Pair.Value.bMovementCriticalFine) ++CriticalFineCount;
	}
	TestTrue(TEXT("Predicted movement marks critical Fine sections"), CriticalFineCount > 0);
	Sources[1].Center.X += 32;
	Runtime.Update(MakeArrayView(Sources), Manifest, Settings, Interest, Delta, bHasSnapshot);
	TestTrue(TEXT("Local source movement keeps distant macro"),
		SameSet(Interest.Macro, Direct.Macro));
	Runtime.Update(MakeArrayView(Sources, 1), Manifest, Settings,
		Interest, Delta, bHasSnapshot);
	TestEqual(TEXT("Removed source leaves one source"), Interest.Sources.Num(), 1);
	TestTrue(TEXT("Removed local source keeps distant macro"),
		SameSet(Interest.Macro, Direct.Macro));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingDemandTest,
	"WHFramework.Voxel.Streaming.DemandSeparation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingDemandTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelStreamingSource Source;
	Source.Center = FIntVector::ZeroValue;
	Source.ExactRadius = 0;
	Source.CollisionRadius = 24;
	Source.Capabilities = EVoxelStreamingCapability::Data |
		EVoxelStreamingCapability::Collision;

	const FVoxelInterestManager Manager;
	const FVoxelInterestSet Interest = Manager.Compute(
		MakeArrayView(&Source, 1),
		Manifest,
		FVoxelViewSettings());
	TestTrue(TEXT("Collision demand creates exact data"), Interest.Exact.Contains(FIntVector::ZeroValue));
	const FVoxelExactDemand& Demand = Interest.Exact.FindChecked(FIntVector::ZeroValue);
	TestTrue(TEXT("Collision demand is marked collision"), Demand.bCollision);
	TestFalse(TEXT("Collision-only demand is not in data warmup"), Demand.bWarmupData);
	TestTrue(TEXT("Collision demand is in warmup collision"), Demand.bWarmupCollision);
	TestFalse(TEXT("Collision-only demand is not marked exact"), Demand.bExact);
	TestFalse(TEXT("Collision demand does not imply fine render"), Demand.bFineRender);
	TestTrue(TEXT("Non-render source creates no voxel proxies"), Interest.VoxelProxy.IsEmpty());
	TestTrue(TEXT("Non-render source creates no surface tiles"), Interest.Surface.IsEmpty());
	TestTrue(TEXT("Non-render source creates no macro tiles"), Interest.Macro.IsEmpty());

	Source.ExactRadius = 64;
	FVoxelViewSettings SmallWarmupSettings;
	Source.Capabilities |= EVoxelStreamingCapability::Readiness;
	Source.View.WarmupDataRadiusCells = 12; // 600 cm at a 50 cm cell size.
	Source.View.WarmupCollisionRadiusCells = 12;
	const FVoxelInterestSet SmallWarmup = Manager.Compute(
		MakeArrayView(&Source, 1), Manifest, SmallWarmupSettings);
	TestTrue(TEXT("Warmup includes the section containing a boundary-aligned source"),
		SmallWarmup.Warmup.Contains(FIntVector::ZeroValue));
	TestTrue(TEXT("Warmup collision includes the source section"),
		SmallWarmup.Exact.FindChecked(FIntVector::ZeroValue).bWarmupCollision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingRenderScopeTest,
	"WHFramework.Voxel.Streaming.RenderScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingRenderScopeTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelViewSettings Settings;
	FVoxelStreamingSource Source;
	Source.Center = FIntVector::ZeroValue;
	Source.ExactRadius = 32;
	Source.CollisionRadius = 32;
	Source.VerticalExactRadius = 16;
	Source.Capabilities = EVoxelStreamingCapability::Data |
		EVoxelStreamingCapability::Collision |
		EVoxelStreamingCapability::FineVisual;

	const FVoxelInterestManager Manager;
	const FVoxelInterestSet FineOnly = Manager.Compute(
		MakeArrayView(&Source, 1),
		Manifest,
		Settings);
	bool bHasFine = false;
	for (const TPair<FIntVector, FVoxelExactDemand>& Pair : FineOnly.Exact)
	{
		bHasFine |= Pair.Value.bFineRender;
	}
	TestTrue(TEXT("FineOnly creates fine demand"), bHasFine);
	TestTrue(TEXT("FineOnly creates no voxel proxies"), FineOnly.VoxelProxy.IsEmpty());
	TestTrue(TEXT("FineOnly creates no surface tiles"), FineOnly.Surface.IsEmpty());
	TestTrue(TEXT("FineOnly creates no macro tiles"), FineOnly.Macro.IsEmpty());

	Source.Capabilities |= EVoxelStreamingCapability::WorldVisual;
	const FVoxelInterestSet Full = Manager.Compute(
		MakeArrayView(&Source, 1),
		Manifest,
		Settings);
	TestFalse(TEXT("Full creates voxel proxies"), Full.VoxelProxy.IsEmpty());
	TestFalse(TEXT("Full creates surface tiles"), Full.Surface.IsEmpty());
	TestFalse(TEXT("Full creates macro tiles"), Full.Macro.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelIndependentSourceViewTest,
	"WHFramework.Voxel.Streaming.IndependentSourceView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelIndependentSourceViewTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelViewSettings Settings;
	FVoxelStreamingSource Sources[2];
	Sources[0].Id = FGuid::NewGuid();
	Sources[0].Capabilities = EVoxelStreamingCapability::Data |
		EVoxelStreamingCapability::Collision |
		EVoxelStreamingCapability::FineVisual |
		EVoxelStreamingCapability::WorldVisual;
	Sources[0].Priority = 20;
	Sources[0].View.FineRadiusCells = 32;
	Sources[0].View.FinePreloadCells = 0;
	Sources[0].View.VoxelProxyRadiusCells = 128;
	Sources[0].View.SurfaceRadiusCells = 512;
	Settings.MacroRadiusCells = 4096;

	Sources[1].Id = FGuid::NewGuid();
	Sources[1].Purpose = EVoxelStreamingSourcePurpose::DebugPOI;
	Sources[1].Priority = -25;
	Sources[1].Center = FIntVector(1024, 0, 0);
	Sources[1].Capabilities = EVoxelStreamingCapability::Data |
		EVoxelStreamingCapability::FineVisual |
		EVoxelStreamingCapability::LocalRefinement;
	Sources[1].View.FineRadiusCells = 48;
	Sources[1].View.FineVerticalRadiusCells = 64;
	Sources[1].View.FinePreloadCells = 0;
	Sources[1].View.VoxelProxyRadiusCells = 10000;

	Sources[1].ExactRadius = -1;
	Sources[1].CollisionRadius = 0;
	Sources[1].SimulationRadius = 0;
	Sources[1].VerticalExactRadius = 0;

	const FVoxelInterestManager Manager;
	const FVoxelInterestSet DebugOnly = Manager.Compute(MakeArrayView(&Sources[1], 1), Manifest, Settings);
	const FVoxelExactDemand* DebugDemand = DebugOnly.Exact.Find(FIntVector(64, 0, 0));
	TestTrue(TEXT("Debug source requests its own fine section"), DebugDemand && DebugDemand->bFineRender);
	if (DebugDemand)
	{
		TestFalse(TEXT("Debug fine does not request exact interaction"), DebugDemand->bExact);
		TestFalse(TEXT("Debug fine does not request collision"), DebugDemand->bCollision);
		TestEqual(TEXT("Debug fine uses numeric source priority"), DebugDemand->Priority, -25);
	}
	TestFalse(TEXT("Debug source retains local voxel proxy ancestors"), DebugOnly.VoxelProxy.IsEmpty());
	bool bProxyRemainsLocal = true;
	for (const FVoxelViewKey& Key : DebugOnly.VoxelProxy)
	{
		const FIntVector BoundsMin = Key.GetBounds().Min;
		const FIntVector BoundsMax = Key.GetBounds().Max;
		const FIntVector Center = (BoundsMin + BoundsMax) / 2;
		bProxyRemainsLocal &= Key.Level == 1 &&
			FMath::Abs(Center.X - Sources[1].Center.X) < 256 &&
			FMath::Abs(Center.Y - Sources[1].Center.Y) < 256;
	}
	TestTrue(TEXT("Debug proxy ignores the global radius and stays near its fine island"), bProxyRemainsLocal);
	TestTrue(TEXT("Debug source does not request surface"), DebugOnly.Surface.IsEmpty());
	TestTrue(TEXT("Debug source does not request macro"), DebugOnly.Macro.IsEmpty());

	const FVoxelInterestSet PlayerOnly = Manager.Compute(MakeArrayView(&Sources[0], 1), Manifest, Settings);
	const FVoxelInterestSet Combined = Manager.Compute(MakeArrayView(Sources), Manifest, Settings);
	const FVoxelSourceInterest& PlayerInterest = Combined.Sources.FindChecked(Sources[0].Id);
	const FVoxelSourceInterest& DebugInterest = Combined.Sources.FindChecked(Sources[1].Id);
	TestTrue(TEXT("Fine planning bounds stay separate for distant sources"),
		PlayerInterest.FineBounds.IsValid() && DebugInterest.FineBounds.IsValid() &&
		PlayerInterest.FineBounds.Max.X < DebugInterest.FineBounds.Min.X);
	bool bBoundsContainFineData = true;
	for (const auto& Pair : Combined.Sources)
	{
		for (const FIntVector& Section : Pair.Value.FineDataSections)
		{
			const FIntVector Min = Section * 16;
			bBoundsContainFineData &= Pair.Value.FineBounds.Contains(Min) &&
				Pair.Value.FineBounds.Contains(Min + FIntVector(15));
		}
	}
	TestTrue(TEXT("Planning bounds contain each source's Fine data and its neighbours"), bBoundsContainFineData);
	bool bEveryFinePrioritized = true;
	for (const auto& Pair : Combined.Exact)
	{
		if (Pair.Value.bFineRender)
		{
			bEveryFinePrioritized &= Pair.Value.Priority != MAX_int32;
		}
	}
	TestTrue(TEXT("Merged fine partition has source priorities"), bEveryFinePrioritized);
	TestEqual(TEXT("Debug source does not add surface tiles"), Combined.Surface.Num(), PlayerOnly.Surface.Num());
	TestEqual(TEXT("Debug source does not add macro tiles"), Combined.Macro.Num(), PlayerOnly.Macro.Num());
	const FVoxelExactDemand* CombinedDemand = Combined.Exact.Find(FIntVector(64, 0, 0));
	TestTrue(TEXT("Debug fine remains requested with player source"), CombinedDemand && CombinedDemand->bFineRender);
	if (CombinedDemand)
	{
		TestEqual(TEXT("Combined demand keeps debug numeric priority"), CombinedDemand->Priority, -25);
	}
	TestTrue(TEXT("Admission order follows numeric priority"),
		!Combined.Admissions.IsEmpty() && Combined.Admissions[0].Priority == -25);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingLodSymmetryTest,
	"WHFramework.Voxel.Streaming.LodSymmetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingLodSymmetryTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelViewSettings Settings;
	FVoxelStreamingSource Source;
	Source.Capabilities = EVoxelStreamingCapability::Data |
		EVoxelStreamingCapability::Collision |
		EVoxelStreamingCapability::FineVisual |
		EVoxelStreamingCapability::WorldVisual;
	Source.View.FineRadiusCells = 64;
	Source.View.FinePreloadCells = 0;
	Source.View.VoxelProxyRadiusCells = 256;
	Source.View.SurfaceRadiusCells = 1024;
	Settings.MacroRadiusCells = 16000;
	Settings.MaximumSurfaceTiles = 256;
	Settings.MaximumMacroTiles = 128;
	Source.ExactRadius = 64;
	Source.Center = FIntVector::ZeroValue;
	const FVoxelInterestManager Manager;
	const FVoxelInterestSet Interest = Manager.Compute(MakeArrayView(&Source, 1), Manifest, Settings);
	TestTrue(TEXT("Surface obeys tile budget"), Interest.Surface.Num() <= Settings.MaximumSurfaceTiles);
	TestTrue(TEXT("Macro obeys tile budget"), Interest.Macro.Num() <= Settings.MaximumMacroTiles);
	TestFalse(TEXT("Surface coverage remains available"), Interest.Surface.IsEmpty());
	TestFalse(TEXT("Macro coverage remains available"), Interest.Macro.IsEmpty());
	for (const FVoxelSurfaceTileKey& Key : Interest.Surface)
	{
		TestTrue(TEXT("Surface reflects across player X"), Interest.Surface.Contains({ FIntPoint(-Key.Coordinate.X - 1, Key.Coordinate.Y), Key.Level }));
		TestTrue(TEXT("Surface reflects across player Y"), Interest.Surface.Contains({ FIntPoint(Key.Coordinate.X, -Key.Coordinate.Y - 1), Key.Level }));
	}
	for (const FVoxelMacroTileKey& Key : Interest.Macro)
	{
		TestTrue(TEXT("Macro reflects across player X"), Interest.Macro.Contains({ FIntPoint(-Key.Coordinate.X - 1, Key.Coordinate.Y), Key.Level }));
		TestTrue(TEXT("Macro reflects across player Y"), Interest.Macro.Contains({ FIntPoint(Key.Coordinate.X, -Key.Coordinate.Y - 1), Key.Level }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelResidencyPinTest,
	"WHFramework.Voxel.Streaming.ResidencyPin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelResidencyPinTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldRuntime Runtime(4, false, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	FVoxelTaskScheduler Scheduler;
	const FIntVector SectionKey(3, -2, 1);
	FVoxelSection* Section = Runtime.FindOrAllocate(SectionKey, 0);
	TestNotNull(TEXT("Residency test section allocates"), Section);
	if (!Section)
	{
		return false;
	}
	Section->PinCount.Store(1);
	int32 Evictions = 0;
	FVoxelResidencyManager Manager(Runtime, Scheduler, [&Evictions](const FIntVector& InSection)
	{
		(void)InSection;
		++Evictions;
	});
	Manager.SetEvictGraceFrames(0);
	Manager.Tick({}, 1, 0.0);
	TestNotNull(TEXT("Pinned section is retained"), Runtime.FindSection(SectionKey));
	TestEqual(TEXT("Pinned section does not run eviction callback"), Evictions, 0);

	Section->PinCount.Store(0);
	const uint64 PreviousFrameCounter = GFrameCounter;
	GFrameCounter += 15;
	Manager.Tick({}, 1, 0.0);
	GFrameCounter = PreviousFrameCounter;
	TestNull(TEXT("Unpinned undemanded section evicts"), Runtime.FindSection(SectionKey));
	TestEqual(TEXT("Eviction callback runs once"), Evictions, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelViewResidencyHandoffTest,
	"WHFramework.Voxel.Streaming.View.ResidencyHandoff", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelViewResidencyHandoffTest::RunTest(const FString& Parameters)
{
	FVoxelWorldRuntime Runtime(4, true, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	FVoxelTaskScheduler Scheduler;
	const FIntVector Key(-3, 2, 1);
	Runtime.FindOrAllocate(Key, 0);
	bool bAwaitingReplacement = true;
	int32 Evictions = 0;
	FVoxelResidencyManager Manager(Runtime, Scheduler,
		[&Evictions](const FIntVector&) { ++Evictions; },
		[&bAwaitingReplacement](const FIntVector&) { return bAwaitingReplacement; });
	Manager.SetEvictGraceFrames(0);
	const uint64 PreviousFrame = GFrameCounter;
	GFrameCounter += 15;
	Manager.Tick({}, 1, 0.0);
	TestNotNull(TEXT("Leaving demand does not unload geometry still awaiting replacement"), Runtime.FindSection(Key));
	TestEqual(TEXT("Retained boundary data does not trigger invalidation"), Evictions, 0);
	bAwaitingReplacement = false;
	GFrameCounter += 15;
	Manager.Tick({}, 1, 0.0);
	TestNull(TEXT("Completed handoff releases data without another interest change"), Runtime.FindSection(Key));
	TestEqual(TEXT("Completed handoff evicts exactly once"), Evictions, 1);
	GFrameCounter = PreviousFrame;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPersistedResidencyTest, "WHFramework.Voxel.Streaming.PersistedResidency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPersistedResidencyTest::RunTest(const FString& InParameters)
{
	FVoxelWorldRuntime Runtime(4, true, VoxelTest::MakeRegistry(), VoxelTest::MakeGenerator());
	FVoxelTaskScheduler Scheduler;
	const FIntVector Key(-3, 2, 1);
	FVoxelSection* Section = Runtime.FindOrAllocate(Key, 0);
	if (!TestNotNull(TEXT("Edited section allocates"), Section))
	{
		return false;
	}
	Section->CommittedRevision = 7;
	Section->PersistedRevision = 6;
	Runtime.GetChangeIndex().SetModified(Key, true);
	FVoxelResidencyManager Manager(Runtime, Scheduler, {});
	Manager.SetEvictGraceFrames(0);
	const uint64 PreviousFrame = GFrameCounter;
	GFrameCounter += 15;
	Manager.Tick({}, 1, 0.0);
	TestNotNull(TEXT("Unpersisted edits remain resident"), Runtime.FindSection(Key));
	Runtime.MarkCommitted(Key, 6);
	GFrameCounter += 15;
	Manager.Tick({}, 1, 0.0);
	TestNotNull(TEXT("Stale save completion cannot permit eviction"), Runtime.FindSection(Key));
	Runtime.MarkCommitted(Key, 7);
	GFrameCounter += 15;
	Manager.Tick({}, 1, 0.0);
	GFrameCounter = PreviousFrame;
	TestNull(TEXT("Persisted modified section may unload"), Runtime.FindSection(Key));
	TestTrue(TEXT("Eviction retains the persistent change index"), Runtime.GetChangeIndex().IsModified(Key));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelFineLodHysteresisTest,
	"WHFramework.Voxel.Streaming.FineLodHysteresis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineLodHysteresisTest::RunTest(const FString& InParameters)
{
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelViewSettings Settings;
	FVoxelStreamingSource Source;
	Source.View.FineRadiusCells = 64;
	Source.View.FinePreloadCells = 0;
	Source.ExactRadius = 64;
	Source.VerticalExactRadius = 16;
	Source.Capabilities = EVoxelStreamingCapability::Data |
		EVoxelStreamingCapability::Collision |
		EVoxelStreamingCapability::FineVisual;
	const FVoxelInterestManager Manager;
	const FIntVector Key(3, 0, 0);
	const FVoxelInterestSet Initial = Manager.Compute(MakeArrayView(&Source, 1), Manifest, Settings);
	TestTrue(TEXT("Approaching enters fine LOD"), Initial.Exact.FindChecked(Key).bFineRender);
	Source.Center.X = -16;
	const FVoxelInterestSet Retained = Manager.Compute(MakeArrayView(&Source, 1), Manifest, Settings, &Initial);
	TestTrue(TEXT("Small retreat retains fine LOD"), Retained.Exact.FindChecked(Key).bFineRender);
	Source.Center.X = -33;
	const FVoxelInterestSet Retired = Manager.Compute(MakeArrayView(&Source, 1), Manifest, Settings, &Retained);
	const FVoxelExactDemand* Demand = Retired.Exact.Find(Key);
	TestFalse(TEXT("Retreat beyond hysteresis releases fine LOD"), Demand && Demand->bFineRender);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingViewNearestFirstTest,
	"WHFramework.Voxel.Streaming.View.NearestFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingViewNearestFirstTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TArray<FVoxelViewAdmission> Admissions;
	Admissions.Add({ EVoxelViewAdmissionKind::Macro, 512.0 });
	Admissions.Add({ EVoxelViewAdmissionKind::Surface, 32.0 });
	Admissions.Add({ EVoxelViewAdmissionKind::VoxelProxy, 128.0 });
	FVoxelViewManager::SortAdmissionsByPriority(Admissions);
	TestEqual(TEXT("Nearest admission is first"), Admissions[0].DistanceCells, 32.0);
	TestEqual(TEXT("Middle admission is second"), Admissions[1].DistanceCells, 128.0);
	TestEqual(TEXT("Farthest admission is last"), Admissions[2].DistanceCells, 512.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingViewFrontierBlocksFarTileTest,
	"WHFramework.Voxel.Streaming.View.FrontierBlocksFarTile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingViewFrontierBlocksFarTileTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TArray<FVoxelViewAdmission> Admissions = {
		{ EVoxelViewAdmissionKind::Fine, 64.0 },
		{ EVoxelViewAdmissionKind::Surface, 256.0 }
	};
	const double Frontier = FVoxelViewManager::ResolveAdmissionFrontier(
		Admissions,
		[](const FVoxelViewAdmission&) { return false; },
		64.0);
	TestEqual(TEXT("Nearest unfinished ring defines the frontier"), Frontier, 128.0);
	TestTrue(TEXT("Far tile is outside the frontier"), Admissions[1].DistanceCells > Frontier);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingViewFrontierAdvancesTest,
	"WHFramework.Voxel.Streaming.View.FrontierAdvances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingViewFrontierAdvancesTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TArray<FVoxelViewAdmission> Admissions = {
		{ EVoxelViewAdmissionKind::Fine, 32.0 },
		{ EVoxelViewAdmissionKind::VoxelProxy, 96.0 },
		{ EVoxelViewAdmissionKind::Surface, 192.0 }
	};
	const double Initial = FVoxelViewManager::ResolveAdmissionFrontier(
		Admissions,
		[](const FVoxelViewAdmission&) { return false; },
		64.0);
	const double Advanced = FVoxelViewManager::ResolveAdmissionFrontier(
		Admissions,
		[](const FVoxelViewAdmission& Admission) { return Admission.DistanceCells < 192.0; },
		64.0);
	TestEqual(TEXT("Initial frontier covers only the nearest band"), Initial, 96.0);
	TestEqual(TEXT("Ready near rings advance the frontier"), Advanced, 256.0);
	TestTrue(TEXT("Far ring becomes admissible"), Admissions[2].DistanceCells <= Advanced);
	const TArray<FVoxelViewAdmission> Handoff = {
		{ EVoxelViewAdmissionKind::Fine, 32.0 },
		{ EVoxelViewAdmissionKind::Fine, 160.0 },
		{ EVoxelViewAdmissionKind::VoxelProxy, 192.0 }
	};
	const double HandoffFrontier = FVoxelViewManager::ResolveAdmissionFrontier(
		Handoff,
		[](const FVoxelViewAdmission& Admission)
		{
			return Admission.Kind != EVoxelViewAdmissionKind::Fine || Admission.DistanceCells == 32.0;
		}, 64.0);
	TestTrue(TEXT("Built near mesh allows remaining fine siblings before visual handoff"),
		Handoff[1].DistanceCells <= HandoffFrontier);
	TestEqual(TEXT("Uncommitted fine meshes still block the proxy stage"),
		FVoxelViewManager::ResolveActiveAdmissionKind(Handoff,
			[](const FVoxelViewAdmission&) { return false; }), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingViewDistanceScoreTest,
	"WHFramework.Voxel.Streaming.View.DistanceScore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingViewDistanceScoreTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelStreamingSource Source;
	Source.Center = FIntVector::ZeroValue;
	Source.ExactRadius = 64;
	Source.CollisionRadius = 64;
	Source.Capabilities = EVoxelStreamingCapability::Data |
		EVoxelStreamingCapability::Collision |
		EVoxelStreamingCapability::FineVisual;
	const FVoxelInterestSet Interest = FVoxelInterestManager().Compute(
		MakeArrayView(&Source, 1), Manifest, FVoxelViewSettings());
	const FVoxelExactDemand* Center = Interest.Exact.Find(FIntVector::ZeroValue);
	TestNotNull(TEXT("Center demand exists"), Center);
	double MinimumDistance = TNumericLimits<double>::Max();
	double MaximumDistance = 0.0;
	for (const TPair<FIntVector, FVoxelExactDemand>& Pair : Interest.Exact)
	{
		TestTrue(TEXT("Demand distance is finite"), FMath::IsFinite(Pair.Value.DistanceCells));
		MinimumDistance = FMath::Min(MinimumDistance, Pair.Value.DistanceCells);
		MaximumDistance = FMath::Max(MaximumDistance, Pair.Value.DistanceCells);
	}
	if (Center)
	{
		TestEqual(TEXT("Observer section has the minimum center distance"), Center->DistanceCells, MinimumDistance);
	}
	TestTrue(TEXT("Off-center demand carries a larger real distance score"), MaximumDistance > MinimumDistance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingSchedulerVisualDistanceFirstTest,
	"WHFramework.Voxel.Streaming.Scheduler.VisualDistanceFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingSchedulerVisualDistanceFirstTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelTaskRequest Near;
	Near.WorkClass = EVoxelWorkClass::Background;
	Near.DistanceScore = 32.0;
	FVoxelTaskRequest Far;
	Far.WorkClass = EVoxelWorkClass::Visible;
	Far.DistanceScore = 512.0;
	TestTrue(TEXT("Near visual work outranks far visual work"), FVoxelTaskScheduler::IsHigherPriority(Near, Far));
	TestFalse(TEXT("Far visual work does not outrank near visual work"), FVoxelTaskScheduler::IsHigherPriority(Far, Near));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingSchedulerCriticalFirstTest,
	"WHFramework.Voxel.Streaming.Scheduler.CriticalFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingSchedulerCriticalFirstTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelTaskRequest Near;
	Near.Kind = EVoxelTaskKind::GenerateExactBase;
	Near.WorkClass = EVoxelWorkClass::ExactData;
	Near.DistanceScore = 32.0;
	FVoxelTaskRequest Far;
	Far.Kind = EVoxelTaskKind::BuildCollision;
	Far.WorkClass = EVoxelWorkClass::Critical;
	Far.DistanceScore = 512.0;
	TestTrue(TEXT("Critical support outranks ordinary nearer generation"), FVoxelTaskScheduler::IsHigherPriority(Far, Near));
	TestFalse(TEXT("Ordinary generation cannot starve critical support"), FVoxelTaskScheduler::IsHigherPriority(Near, Far));
	FVoxelTaskRequest Visual;
	Visual.Kind = EVoxelTaskKind::BuildVoxelProxy;
	Visual.WorkClass = EVoxelWorkClass::Visible;
	Visual.DistanceScore = 0.0;
	TestTrue(TEXT("Critical work outranks zero-distance visual work"), FVoxelTaskScheduler::IsHigherPriority(Far, Visual));
	TestTrue(TEXT("Required data outranks visual work"), FVoxelTaskScheduler::IsHigherPriority(Near, Visual));
	const FVoxelTaskRequest* Requests[] = { &Near, &Far, &Visual };
	for (const FVoxelTaskRequest* First : Requests)
	{
		for (const FVoxelTaskRequest* Second : Requests)
		{
			for (const FVoxelTaskRequest* Third : Requests)
			{
				if (FVoxelTaskScheduler::IsHigherPriority(*First, *Second) && FVoxelTaskScheduler::IsHigherPriority(*Second, *Third))
				{
					TestTrue(TEXT("Mixed data collision and view priorities are transitive"), FVoxelTaskScheduler::IsHigherPriority(*First, *Third));
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineIndependentRadiusTest, "WHFramework.Voxel.Streaming.FineRadiusIndependentOfInteraction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineIndependentRadiusTest::RunTest(const FString& InParameters)
{
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -256;
	Manifest.Settings.MaxZ = 256;
	FVoxelViewSettings Settings;
	FVoxelStreamingSource Source;
	Source.View.FineRadiusCells = 96;
	Source.View.FinePreloadCells = 16;
	Source.View.FineVerticalRadiusCells = 32;
	Source.ExactRadius = 16;
	Source.CollisionRadius = 16;
	Source.VerticalExactRadius = 16;
	Source.Capabilities = EVoxelStreamingCapability::Data |
		EVoxelStreamingCapability::Collision |
		EVoxelStreamingCapability::FineVisual;
	const FVoxelInterestSet Interest = FVoxelInterestManager().Compute(MakeArrayView(&Source, 1), Manifest, Settings);
	for (const FIntVector Key : { FIntVector(5, 0, 0), FIntVector(-6, 0, 0), FIntVector(0, 0, 1) })
	{
		const FVoxelExactDemand* Demand = Interest.Exact.Find(Key);
		TestTrue(TEXT("Configured fine volume includes nearby horizontal and vertical sections"), Demand && Demand->bFineRender);
		TestFalse(TEXT("Fine range does not enlarge collision or simulation"), Demand && (Demand->bCollision || Demand->bSimulation));
	}
	const FVoxelExactDemand* DeepDemand = Interest.Exact.Find(FIntVector(0, 0, 5));
	TestFalse(TEXT("Fine vertical radius excludes deep invisible sections"), DeepDemand && DeepDemand->bFineRender);
	TestFalse(TEXT("Deep invisible sections never enter fine admissions"),
		Interest.FineSections && Interest.FineSections->Contains(FIntVector(0, 0, 5)));
	for (const auto& Pair : Interest.Exact)
	{
		if (!Pair.Value.bFineRender) continue;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (const int32 Sign : { -1, 1 })
			{
				FIntVector Neighbor = Pair.Key;
				Neighbor[Axis] += Sign;
				TestTrue(TEXT("Every fine boundary has a requested real data halo"), Interest.Exact.Contains(Neighbor));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSchedulerAdmissionTest, "WHFramework.Voxel.Streaming.Scheduler.PriorityAdmission", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSchedulerAdmissionTest::RunTest(const FString& InParameters)
{
	FVoxelTaskScheduler Scheduler;
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 1;
	Budget.MaxPendingTasks = 1;
	Scheduler.SetBudget(Budget);
	auto MakeRequest = [](const uint64 Token, const EVoxelWorkClass WorkClass, const double Distance)
	{
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::GenerateExactBase;
		Request.WorkClass = WorkClass;
		Request.Stamp.WorldEpoch = 1;
		Request.Stamp.Token = Token;
		Request.DistanceScore = Distance;
		Request.ReservedBytes = 1024;
		Request.Execute = [](const TAtomic<bool>& Cancel)
		{
			while (!Cancel.Load())
			{
				FPlatformProcess::Sleep(0.001f);
			}
			return FVoxelTaskResult();
		};
		return Request;
	};
	FVoxelTaskRequest Running = MakeRequest(1, EVoxelWorkClass::Visible, 1.0);
	TestTrue(TEXT("Worker is occupied"), Scheduler.Enqueue(MoveTemp(Running)));
	FVoxelTaskRequest Far = MakeRequest(2, EVoxelWorkClass::Visible, 1000.0);
	const FVoxelTaskStamp FarStamp = Far.Stamp;
	int32 CanceledCallbacks = 0;
	Far.Apply = [&CanceledCallbacks](FVoxelTaskResult&& Result)
	{
		CanceledCallbacks += Result.bCanceled ? 1 : 0;
	};
	TestTrue(TEXT("Far work fills the pending queue"), Scheduler.Enqueue(MoveTemp(Far)));
	FVoxelTaskRequest Critical = MakeRequest(3, EVoxelWorkClass::Critical, 10.0);
	const FVoxelTaskStamp CriticalStamp = Critical.Stamp;
	TestTrue(TEXT("Critical work can enter a full queue"), Scheduler.Enqueue(MoveTemp(Critical)));
	TestFalse(TEXT("Displaced work is removed from active bookkeeping"), Scheduler.Has(FarStamp, EVoxelTaskKind::GenerateExactBase));
	TestTrue(TEXT("Critical work remains queued"), Scheduler.Has(CriticalStamp, EVoxelTaskKind::GenerateExactBase));
	Scheduler.Tick([](FVoxelTaskResult&&) {});
	TestEqual(TEXT("Displaced owner receives cancellation exactly once"), CanceledCallbacks, 1);
	TestEqual(TEXT("Pending queue stays bounded"), Scheduler.GetDiagnostics().Pending, 1);
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSchedulerTerrainReserveTest,
	"WHFramework.Voxel.Streaming.Scheduler.TerrainLaneAdmission", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSchedulerTerrainReserveTest::RunTest(const FString& Parameters)
{
	FVoxelTaskScheduler Scheduler;
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 1;
	Budget.MaxPendingTasks = 16;
	Scheduler.SetBudget(Budget);
	auto MakeRequest = [](const uint64 Token, const EVoxelTaskKind Kind, const double Distance)
	{
		FVoxelTaskRequest Request;
		Request.Kind = Kind;
		Request.WorkClass = EVoxelWorkClass::Visible;
		Request.Stamp.Token = Token;
		Request.DistanceScore = Distance;
		Request.ReservedBytes = 1024;
		Request.Execute = [](const TAtomic<bool>& Cancel)
		{
			while (!Cancel.Load()) FPlatformProcess::Sleep(0.001f);
			return FVoxelTaskResult();
		};
		return Request;
	};
	Scheduler.Enqueue(MakeRequest(1, EVoxelTaskKind::GenerateExactBase, 0.0));
	for (int32 Index = 0; Index < 7; ++Index)
	{
		TestTrue(TEXT("Ordinary generation uses its available slots"), Scheduler.Enqueue(MakeRequest(2 + Index, EVoxelTaskKind::GenerateExactBase, Index)));
	}
	TestFalse(TEXT("Generation cannot consume every representation slot"), Scheduler.Enqueue(MakeRequest(20, EVoxelTaskKind::GenerateExactBase, 100.0)));
	const EVoxelTaskKind Kinds[] = { EVoxelTaskKind::BuildFineMesh, EVoxelTaskKind::BuildVoxelProxy, EVoxelTaskKind::BuildSurface, EVoxelTaskKind::BuildMacro };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		TestTrue(TEXT("Each representation can enter under generation pressure"), Scheduler.Enqueue(MakeRequest(30 + Index, Kinds[Index], 10000.0)));
	}
	const EVoxelTaskKind DataKinds[] = {EVoxelTaskKind::GenerateVoxelProxy, EVoxelTaskKind::GenerateSurface, EVoxelTaskKind::GenerateMacro};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FVoxelTaskRequest Request = MakeRequest(40 + Index, DataKinds[Index], 10000.0);
		Request.WorkClass = EVoxelWorkClass::Prefetch;
		TestTrue(TEXT("Prefetch data shares its representation reserve"), Scheduler.Enqueue(MoveTemp(Request)));
	}
	FVoxelTaskRequest Publication = MakeRequest(50, EVoxelTaskKind::BuildViewCoverage, 10000.0);
	Publication.bPublicationContinuation = true;
	Publication.TerrainStage = 2;
	TestTrue(TEXT("An admitted publication retains queue space under continuous generation pressure"),
		Scheduler.Enqueue(MoveTemp(Publication)));
	for (int32 Index = 0; Index < 24; ++Index)
	{
		FVoxelTaskRequest Request = MakeRequest(100 + Index, EVoxelTaskKind::GenerateExactBase, 0.0);
		Request.WorkClass = EVoxelWorkClass::Critical;
		Scheduler.Enqueue(MoveTemp(Request));
	}
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FVoxelTaskStamp Stamp;
		Stamp.Token = 40 + Index;
		TestTrue(TEXT("Critical data pressure preserves each distant data lane"), Scheduler.Has(Stamp, DataKinds[Index]));
	}
	FVoxelTaskStamp PublicationStamp;
	PublicationStamp.Token = 50;
	TestTrue(TEXT("Critical data pressure preserves the bounded publication continuation"),
		Scheduler.Has(PublicationStamp, EVoxelTaskKind::BuildViewCoverage));
	TestTrue(TEXT("Shared pending queue remains bounded"), Scheduler.GetDiagnostics().Pending <= Budget.MaxPendingTasks);
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelEmptyMeshApplyTest,
	"WHFramework.Voxel.Streaming.Scheduler.EmptyMeshApply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelEmptyMeshApplyTest::RunTest(const FString& Parameters)
{
	FVoxelTaskResult Result;
	Result.Kind = EVoxelTaskKind::BuildFineMesh;
	Result.bSuccess = true;
	Result.FineMesh = MakeShared<FVoxelSectionMeshResult>();
	TestFalse(TEXT("Known empty sections do not block heavy mesh publication"), Result.HasHeavyApply());
	Result.FineMesh->Batches.AddDefaulted_GetRef().Mesh.Triangles = { 0, 1, 2 };
	TestTrue(TEXT("Renderable mesh still obeys the heavy result budget"), Result.HasHeavyApply());
	Result.bCanceled = true;
	TestFalse(TEXT("Canceled geometry does not consume publication quota"), Result.HasHeavyApply());
	Result.bCanceled = false;
	Result.Kind = EVoxelTaskKind::BuildVoxelProxy;
	Result.VoxelProxyMesh = MakeShared<FVoxelSectionMeshResult>();
	TestFalse(TEXT("Empty proxy does not consume heavy publication quota"), Result.HasHeavyApply());
	Result.VoxelProxyMesh->Batches.AddDefaulted_GetRef().Mesh.Triangles = { 0, 1, 2 };
	TestTrue(TEXT("Visible proxy keeps heavy publication quota"), Result.HasHeavyApply());
	Result.Kind = EVoxelTaskKind::GenerateVoxelProxy;
	TestFalse(TEXT("Prepared data does not publish geometry"), Result.HasHeavyApply());
	Result.Kind = EVoxelTaskKind::BuildCollision;
	TestTrue(TEXT("Collision keeps its heavy apply protection"), Result.HasHeavyApply());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMovingPrioritySchedulerTest,
	"WHFramework.Voxel.Streaming.Scheduler.MovingPriority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMovingPrioritySchedulerTest::RunTest(const FString& Parameters)
{
	TAtomic<bool> Release(false);
	FVoxelTaskScheduler Scheduler;
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 1;
	Scheduler.SetBudget(Budget);
	TArray<uint64> Applied;
	for (uint64 Token = 1; Token <= 3; ++Token)
	{
		FVoxelTaskRequest Request;
		Request.Kind = EVoxelTaskKind::BuildFineMesh;
		Request.WorkClass = EVoxelWorkClass::Visible;
		Request.Stamp.Token = Token;
		Request.DistanceScore = static_cast<double>(Token);
		Request.ReservedBytes = 1024;
		Request.Execute = [&Release](const TAtomic<bool>& Cancel)
		{
			while (!Release.Load() && !Cancel.Load()) FPlatformProcess::Sleep(0.001f);
			FVoxelTaskResult Result;
			Result.bSuccess = !Cancel.Load();
			return Result;
		};
		TestTrue(TEXT("Movement test task enters scheduler"), Scheduler.Enqueue(MoveTemp(Request)));
	}
	Scheduler.UpdatePriorities([](EVoxelTaskKind, const FVoxelTaskStamp& Stamp,
		EVoxelWorkClass& WorkClass, int32&,
		double& Distance, double& Forward)
	{
		WorkClass = Stamp.Token == 3 ? EVoxelWorkClass::Critical : EVoxelWorkClass::Interactive;
		Distance = Stamp.Token == 3 ? 0.0 : 100.0;
		Forward = 0.0;
	});
	TestEqual(TEXT("Promotion updates critical bookkeeping"), Scheduler.CriticalCount(), 1);
	TestEqual(TEXT("Reprioritizing retains pending work"), Scheduler.GetDiagnostics().Pending, 2);
	TestEqual(TEXT("Reprioritizing does not cancel the running task"), Scheduler.GetDiagnostics().Running, 1);
	Release.Store(true);
	const double Deadline = FPlatformTime::Seconds() + 5.0;
	while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([&Applied](FVoxelTaskResult&& Result)
		{
			if (!Result.bCanceled) Applied.Add(Result.Stamp.Token);
		}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	Scheduler.StopAndJoin();
	if (!TestEqual(TEXT("All existing jobs complete exactly once"), Applied.Num(), 3)) return false;
	TestEqual(TEXT("Already running task is preserved"), Applied[0], 1ull);
	TestEqual(TEXT("Newly near task overtakes formerly near queued task"), Applied[1], 3ull);
	TestEqual(TEXT("Formerly near task still completes"), Applied[2], 2ull);
	TestEqual(TEXT("Critical bookkeeping clears after completion"), Scheduler.CriticalCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainStagePriorityTest,
	"WHFramework.Voxel.Streaming.Scheduler.TerrainStagePriority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainStagePriorityTest::RunTest(const FString& Parameters)
{
	FVoxelTaskRequest CriticalTransition;
	CriticalTransition.Kind = EVoxelTaskKind::BuildVolumeTransition;
	CriticalTransition.WorkClass = EVoxelWorkClass::Critical;
	CriticalTransition.SourcePriority = 0;
	CriticalTransition.DistanceScore = 1000.0;
	FVoxelTaskRequest NearProxy;
	NearProxy.Kind = EVoxelTaskKind::BuildVoxelProxy;
	NearProxy.WorkClass = EVoxelWorkClass::Visible;
	NearProxy.SourcePriority = 0;
	TestTrue(TEXT("Critical Fine handoff retains the Fine terrain band ahead of nearer Proxy"),
		FVoxelTaskScheduler::IsHigherPriority(CriticalTransition, NearProxy));
	TestFalse(TEXT("Ordinary Proxy cannot delay Critical Fine handoff"),
		FVoxelTaskScheduler::IsHigherPriority(NearProxy, CriticalTransition));
	FVoxelTaskRequest Fine;
	Fine.Kind = EVoxelTaskKind::BuildFineMesh;
	Fine.WorkClass = EVoxelWorkClass::Visible;
	FVoxelTaskRequest CoarseCoverage;
	CoarseCoverage.Kind = EVoxelTaskKind::BuildViewCoverage;
	CoarseCoverage.TerrainStage = 2;
	CoarseCoverage.bPublicationContinuation = true;
	CoarseCoverage.WorkClass = EVoxelWorkClass::Critical;
	TestTrue(TEXT("Coarse clipping required by Critical Fine inherits the Fine priority band"),
		FVoxelTaskScheduler::IsHigherPriority(CoarseCoverage, Fine));
	CoarseCoverage.WorkClass = EVoxelWorkClass::Visible;
	Fine.WorkClass = EVoxelWorkClass::Critical;
	TestTrue(TEXT("Critical Fine remains ahead of an ordinary publication continuation"),
		FVoxelTaskScheduler::IsHigherPriority(Fine, CoarseCoverage));
	TestTrue(TEXT("An admitted coarse publication can finish before new coarse generation"),
		FVoxelTaskScheduler::IsHigherPriority(CoarseCoverage, NearProxy));
	FVoxelTaskRequest CoarsePlan = CoarseCoverage;
	CoarsePlan.bPublicationContinuation = false;
	Fine.WorkClass = EVoxelWorkClass::Visible;
	Fine.DistanceScore = 0.0;
	CoarseCoverage.DistanceScore = 100000.0;
	TestTrue(TEXT("An admitted handoff finishes ahead of new ordinary Fine visual work"),
		FVoxelTaskScheduler::IsHigherPriority(CoarseCoverage, Fine));
	TestFalse(TEXT("New nearer visual work cannot repeatedly displace an admitted handoff"),
		FVoxelTaskScheduler::IsHigherPriority(Fine, CoarseCoverage));
	FVoxelTaskRequest NewCoverage = CoarseCoverage;
	CoarseCoverage.QueuedAt = 1.0;
	NewCoverage.QueuedAt = 2.0;
	NewCoverage.DistanceScore = 0.0;
	TestTrue(TEXT("Earlier admitted handoffs retain their place ahead of newer nearby handoffs"),
		FVoxelTaskScheduler::IsHigherPriority(CoarseCoverage, NewCoverage));
	NewCoverage.WorkClass = EVoxelWorkClass::Critical;
	TestTrue(TEXT("Critical Fine handoffs still overtake older ordinary handoffs"),
		FVoxelTaskScheduler::IsHigherPriority(NewCoverage, CoarseCoverage));
	TestTrue(TEXT("Independent coarse coverage planning retains its own terrain stage"),
		FVoxelTaskScheduler::IsHigherPriority(Fine, CoarsePlan));
	FVoxelTaskRequest CriticalCoverage = CoarseCoverage;
	CriticalCoverage.WorkClass = EVoxelWorkClass::Critical;
	CriticalCoverage.DistanceScore = 70.0;
	FVoxelTaskRequest CriticalFine = Fine;
	CriticalFine.WorkClass = EVoxelWorkClass::Critical;
	CriticalFine.DistanceScore = 0.0;
	CriticalFine.ForwardScore = 1.0;
	TestTrue(TEXT("Critical publication completion is not displaced by new nearer Critical Fine meshes"),
		FVoxelTaskScheduler::IsHigherPriority(CriticalCoverage, CriticalFine));
	FVoxelTaskRequest CriticalData = CriticalFine;
	CriticalData.Kind = EVoxelTaskKind::GenerateExactBase;
	CriticalData.DistanceScore = 30.0;
	TestTrue(TEXT("Upstream Critical exact data remains ahead of publication clipping"),
		FVoxelTaskScheduler::IsHigherPriority(CriticalData, CriticalCoverage));
	TestTrue(TEXT("Upstream Critical data precedes new meshes without introducing a cyclic comparison"),
		FVoxelTaskScheduler::IsHigherPriority(CriticalData, CriticalFine));
	FVoxelTaskRequest NewCriticalCoverage = CriticalCoverage;
	NewCriticalCoverage.QueuedAt = 2.0;
	NewCriticalCoverage.DistanceScore = 0.0;
	TestTrue(TEXT("Earlier Critical publication completes before new nearby Critical publication"),
		FVoxelTaskScheduler::IsHigherPriority(CriticalCoverage, NewCriticalCoverage));
	FVoxelTaskRequest PreferredSourceFine = CriticalFine;
	PreferredSourceFine.SourcePriority = CriticalCoverage.SourcePriority - 1;
	TestTrue(TEXT("Higher source priority still precedes another source's publication completion"),
		FVoxelTaskScheduler::IsHigherPriority(PreferredSourceFine, CriticalCoverage));
	const TArray<FVoxelTaskRequest> CriticalOrder{CriticalData, CriticalCoverage, NewCriticalCoverage, CriticalFine};
	for (int32 Earlier = 0; Earlier < CriticalOrder.Num(); ++Earlier)
		for (int32 Later = Earlier + 1; Later < CriticalOrder.Num(); ++Later)
		{
			TestTrue(TEXT("Critical upstream and publication ordering is transitive"),
				FVoxelTaskScheduler::IsHigherPriority(CriticalOrder[Earlier], CriticalOrder[Later]));
			TestFalse(TEXT("Critical upstream and publication ordering is asymmetric"),
				FVoxelTaskScheduler::IsHigherPriority(CriticalOrder[Later], CriticalOrder[Earlier]));
		}
	for (const EVoxelWorkClass FixtureClass : {EVoxelWorkClass::Visible, EVoxelWorkClass::Critical})
	{
		FVoxelTaskScheduler Scheduler;
		FVoxelTaskBudget Budget;
		Budget.MaxConcurrentTasks = 1;
		Budget.MaxPendingTasks = 4;
		Scheduler.SetBudget(Budget);
		TAtomic<bool> Release(false);
		auto Queue = [&](const int32 Token)
		{
			FVoxelTaskRequest Request;
			Request.Kind = Token == 1 ? EVoxelTaskKind::BuildCollision
				: Token == 2 ? EVoxelTaskKind::BuildFineMesh : EVoxelTaskKind::BuildViewCoverage;
			Request.WorkClass = Token == 1 ? EVoxelWorkClass::Critical : FixtureClass;
			Request.bPublicationContinuation = Token == 3;
			Request.Stamp.Token = Token;
			Request.DistanceScore = Token == 3 ? 100000.0 : 0.0;
			Request.ReservedBytes = 1024;
			Request.Execute = [&Release, Token](const TAtomic<bool>& Cancel)
			{
				while (Token == 1 && !Release.Load() && !Cancel.Load()) FPlatformProcess::Sleep(0.001f);
				FVoxelTaskResult Result;
				Result.bSuccess = !Cancel.Load();
				return Result;
			};
			return Scheduler.Enqueue(MoveTemp(Request));
		};
		TestTrue(TEXT("A critical task occupies the handoff ordering fixture"), Queue(1));
		TestTrue(TEXT("New Fine waits while upstream collision occupies the worker"), Queue(2));
		FPlatformProcess::Sleep(1.05f);
		TestTrue(TEXT("A later admitted handoff enters the same Fine terrain band"), Queue(3));
		Release.Store(true);
		TArray<uint64> Order;
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < Deadline)
		{
			Scheduler.Tick([&Order](FVoxelTaskResult&& Result) { Order.Add(Result.Stamp.Token); }, 8.0);
			FPlatformProcess::Sleep(0.001f);
		}
		Scheduler.StopAndJoin();
		TestTrue(TEXT("Ordinary aging and Critical distance cannot undo the handoff dependency order"), Order == TArray<uint64>{1, 3, 2});
	}
	FVoxelTaskRequest LowPriorityPlanning;
	LowPriorityPlanning.Kind = EVoxelTaskKind::ProjectBackground;
	LowPriorityPlanning.WorkClass = EVoxelWorkClass::ExactData;
	LowPriorityPlanning.SourcePriority = 10;
	FVoxelTaskRequest HighPriorityFine = CriticalTransition;
	HighPriorityFine.SourcePriority = -25;
	TestTrue(TEXT("Lower source planning cannot outrank another source's critical Fine"),
		FVoxelTaskScheduler::IsHigherPriority(HighPriorityFine, LowPriorityPlanning));
	FVoxelTaskRequest FinePlanning;
	FinePlanning.Kind = EVoxelTaskKind::BuildGenerationPlan;
	FinePlanning.TerrainStage = 0;
	FinePlanning.WorkClass = EVoxelWorkClass::Visible;
	FinePlanning.SourcePriority = 0;
	TestTrue(TEXT("Fine dependencies retain their consumer's stage before Proxy"),
		FVoxelTaskScheduler::IsHigherPriority(FinePlanning, NearProxy));
	const EVoxelTaskKind Kinds[] = {
		EVoxelTaskKind::GenerateExactBase, EVoxelTaskKind::BuildFineMesh,
		EVoxelTaskKind::GenerateVoxelProxy, EVoxelTaskKind::BuildVoxelProxy,
		EVoxelTaskKind::GenerateSurface, EVoxelTaskKind::BuildSurface,
		EVoxelTaskKind::GenerateMacro, EVoxelTaskKind::BuildMacro
	};
	for (int32 Earlier = 0; Earlier < 8; ++Earlier)
	{
		for (int32 Later = 0; Later < 8; ++Later)
		{
			if (Earlier / 2 >= Later / 2) continue;
			FVoxelTaskRequest Predecessor;
			Predecessor.Kind = Kinds[Earlier];
			Predecessor.WorkClass = EVoxelWorkClass::Prefetch;
			Predecessor.DistanceScore = 100000.0;
			Predecessor.QueuedAt = 100.0;
			FVoxelTaskRequest Successor;
			Successor.Kind = Kinds[Later];
			Successor.WorkClass = EVoxelWorkClass::Visible;
			Successor.DistanceScore = 0.0;
			Successor.QueuedAt = 0.0;
			TestTrue(TEXT("Earlier terrain data and mesh outrank a nearer older later stage"),
				FVoxelTaskScheduler::IsHigherPriority(Predecessor, Successor));
			TestFalse(TEXT("Later terrain stage cannot overtake its predecessor"),
				FVoxelTaskScheduler::IsHigherPriority(Successor, Predecessor));
		}
	}
	for (const int32 WorkerCount : { 1, 16 })
	{
		TAtomic<bool> Release(false);
		TAtomic<int32> Finished(0);
		FVoxelTaskScheduler Scheduler;
		FVoxelTaskBudget Budget;
		Budget.MaxConcurrentTasks = WorkerCount;
		Budget.MaxConcurrentCoarseTerrainTasks = WorkerCount;
		Budget.MaxConcurrentSurfaceTasks = WorkerCount;
		Budget.MaxConcurrentMacroTasks = WorkerCount;
		Budget.MaxCompletedResultsPerFrame = 16;
		Budget.MaxHeavyCompletedResultsPerFrame = 16;
		Scheduler.SetBudget(Budget);
		auto Enqueue = [&](const int32 Index)
		{
			FVoxelTaskRequest Request;
			Request.Kind = Index == 8 ? EVoxelTaskKind::BuildCollision : Kinds[Index];
			Request.WorkClass = Index == 8 ? EVoxelWorkClass::Critical : EVoxelWorkClass::Visible;
			Request.Stamp.Token = Index;
			Request.DistanceScore = 1000.0 - Index;
			Request.ReservedBytes = 1024;
			Request.Execute = [&Release, &Finished](const TAtomic<bool>& Cancel)
			{
				while (!Release.Load() && !Cancel.Load()) FPlatformProcess::Sleep(0.001f);
				FVoxelTaskResult Result;
				Result.bSuccess = !Cancel.Load();
				++Finished;
				return Result;
			};
			TestTrue(TEXT("Stage ordering fixture enters scheduler"), Scheduler.Enqueue(MoveTemp(Request)));
		};
		Enqueue(8);
		for (int32 Index = 7; Index >= 0; --Index) Enqueue(Index);
		if (WorkerCount == 1) FPlatformProcess::Sleep(1.05f);
		Release.Store(true);
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		if (WorkerCount > 1)
		{
			while (Finished.Load() < 9 && FPlatformTime::Seconds() < Deadline) FPlatformProcess::Sleep(0.001f);
			// Execute increments Finished before the worker marks its task complete.
			FPlatformProcess::Sleep(0.5f);
		}
		TArray<int32> Stages;
		while (Scheduler.ActiveCount() > 0 && FPlatformTime::Seconds() < Deadline)
		{
			Scheduler.Tick([&Stages](FVoxelTaskResult&& Result)
			{
				if (Result.Stamp.Token < 8 && !Result.bCanceled) Stages.Add(static_cast<int32>(Result.Stamp.Token) / 2);
			}, 8.0);
			FPlatformProcess::Sleep(0.001f);
		}
		Scheduler.StopAndJoin();
		TestEqual(TEXT("All terrain tasks finish without cancellation"), Stages.Num(), 8);
		for (int32 Index = 1; Index < Stages.Num(); ++Index)
		{
			TestTrue(*FString::Printf(TEXT("Stage application order: workers=%d index=%d previous=%d current=%d"),
				WorkerCount, Index, Stages[Index - 1], Stages[Index]),
				Stages[Index - 1] <= Stages[Index]);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelDataPrefetchSchedulerTest,
	"WHFramework.Voxel.Streaming.Scheduler.DataPrefetch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCriticalCapacitySchedulerTest,
	"WHFramework.Voxel.Streaming.Scheduler.CriticalCapacity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCriticalCapacitySchedulerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FVoxelTaskScheduler Scheduler;
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 2;
	Budget.CriticalReservedTasks = 1;
	Scheduler.SetBudget(Budget);
	TAtomic<bool> ReleaseBackground(false);
	TAtomic<bool> ReleaseCritical(false);
	auto MakeRequest = [&ReleaseBackground, &ReleaseCritical](const uint64 Token, const EVoxelTaskKind Kind,
		const EVoxelWorkClass WorkClass)
	{
		FVoxelTaskRequest Request;
		Request.Kind = Kind;
		Request.WorkClass = WorkClass;
		Request.Stamp.Token = Token;
		Request.ReservedBytes = WorkClass == EVoxelWorkClass::Critical
			? 128ull * 1024ull * 1024ull : 1024;
		Request.Execute = [&ReleaseBackground, &ReleaseCritical, WorkClass](const TAtomic<bool>& Cancel)
		{
			const TAtomic<bool>& Release = WorkClass == EVoxelWorkClass::Critical ? ReleaseCritical : ReleaseBackground;
			while (!Release.Load() && !Cancel.Load()) FPlatformProcess::Sleep(0.001f);
			FVoxelTaskResult Result;
			Result.bSuccess = !Cancel.Load();
			return Result;
		};
		return Request;
	};
	TestTrue(TEXT("First background task starts"), Scheduler.Enqueue(
		MakeRequest(101, EVoxelTaskKind::ProjectBackground, EVoxelWorkClass::Background)));
	TestTrue(TEXT("Second background task fills worker slots"), Scheduler.Enqueue(
		MakeRequest(102, EVoxelTaskKind::ProjectBackground, EVoxelWorkClass::Background)));
	TestTrue(TEXT("Critical task queues behind running work"), Scheduler.Enqueue(
		MakeRequest(103, EVoxelTaskKind::BuildCollision, EVoxelWorkClass::Critical)));
	TestTrue(TEXT("Coarse work queues behind critical work"), Scheduler.Enqueue(
		MakeRequest(104, EVoxelTaskKind::BuildSurface, EVoxelWorkClass::Visible)));
	TestEqual(TEXT("Critical task is pending"), Scheduler.GetDiagnostics().CriticalPending, 1);
	Scheduler.CancelMatching([](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::ProjectBackground && Stamp.Token == 101;
	});
	const double Deadline = FPlatformTime::Seconds() + 5.0;
	while (Scheduler.GetDiagnostics().CriticalAdmissionDeferrals == 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("Coarse admission is deferred while critical needs capacity"),
		Scheduler.GetDiagnostics().CriticalAdmissionDeferrals > 0);
	TestEqual(TEXT("Coarse work stays pending"),
		Scheduler.GetDiagnostics().PendingByKind.FindRef(EVoxelTaskKind::BuildSurface), 1);
	ReleaseBackground.Store(true);
	while (Scheduler.GetDiagnostics().CriticalRunning == 0 && FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.001f);
	}
	TestEqual(TEXT("Critical task takes released slot"), Scheduler.GetDiagnostics().CriticalRunning, 1);
	Scheduler.StopAndJoin();
	return true;
}

bool FVoxelDataPrefetchSchedulerTest::RunTest(const FString& Parameters)
{
	FVoxelTaskScheduler Scheduler;
	FVoxelTaskBudget Budget;
	Budget.MaxConcurrentTasks = 4;
	Scheduler.SetBudget(Budget);
	auto MakeRequest = [](uint64 Token, EVoxelTaskKind Kind, EVoxelWorkClass WorkClass)
	{
		FVoxelTaskRequest Request;
		Request.Kind = Kind;
		Request.WorkClass = WorkClass;
		Request.Stamp.Token = Token;
		Request.ReservedBytes = 1024;
		Request.Execute = [](const TAtomic<bool>& Cancel)
		{
			while (!Cancel.Load()) FPlatformProcess::Sleep(0.001f);
			return FVoxelTaskResult();
		};
		return Request;
	};
	FVoxelTaskRequest Prefetch = MakeRequest(1, EVoxelTaskKind::GenerateVoxelProxy, EVoxelWorkClass::Prefetch);
	FVoxelTaskRequest Fine = MakeRequest(2, EVoxelTaskKind::BuildFineMesh, EVoxelWorkClass::Visible);
	Fine.DistanceScore = 10000.0;
	TestTrue(TEXT("Fine has priority over nearer speculative data"), FVoxelTaskScheduler::IsHigherPriority(Fine, Prefetch));
	TestFalse(TEXT("Speculative data cannot outrank Fine"), FVoxelTaskScheduler::IsHigherPriority(Prefetch, Fine));
	Scheduler.Enqueue(MoveTemp(Fine));
	Scheduler.Enqueue(MoveTemp(Prefetch));
	Scheduler.Enqueue(MakeRequest(3, EVoxelTaskKind::GenerateSurface, EVoxelWorkClass::Prefetch));
	Scheduler.Enqueue(MakeRequest(4, EVoxelTaskKind::GenerateMacro, EVoxelWorkClass::Prefetch));
	FVoxelTaskDiagnostics Stats = Scheduler.GetDiagnostics();
	TestEqual(TEXT("Data starts while Fine is still running"), Stats.RunningByKind.FindRef(EVoxelTaskKind::GenerateVoxelProxy), 1);
	TestEqual(TEXT("Data tasks leave foreground capacity available"), Stats.Running, 3);
	TestEqual(TEXT("Excess speculative data remains queued"), Stats.Pending, 1);
	Scheduler.Enqueue(MakeRequest(5, EVoxelTaskKind::BuildCollision, EVoxelWorkClass::Critical));
	TestEqual(TEXT("New collision work starts without waiting for prefetch"), Scheduler.GetDiagnostics().RunningByKind.FindRef(EVoxelTaskKind::BuildCollision), 1);
	Scheduler.CancelMatching([](EVoxelTaskKind Kind, const FVoxelTaskStamp&)
	{
		return Kind == EVoxelTaskKind::GenerateMacro;
	});
	TestEqual(TEXT("Obsolete queued data is removed"), Scheduler.GetDiagnostics().Pending, 0);
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelMapTileCoverageTest,
	"WHFramework.Voxel.Map.TileCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMapTileCoverageTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TestEqual(TEXT("Near map step"), FVoxelMapTileCache::SelectStep(0.3f), 16);
	TestEqual(TEXT("Regional map step"), FVoxelMapTileCache::SelectStep(0.02f), 512);
	FSceneMapView View;
	View.Center = FVector2D(-25.0, -25.0);
	View.Range = 500000.f;
	View.Yaw = 45.f;
	TArray<FVoxelMapTileKey> Keys;
	FVoxelMapTileCache::VisibleTiles(View, FVector2D(1000.0, 700.0), 25.0, Keys);
	TestTrue(TEXT("Rotated view includes negative tile coordinates"),
		Keys.ContainsByPredicate([](const FVoxelMapTileKey& Key)
		{
			return Key.Coordinate.X < 0 && Key.Coordinate.Y < 0;
		}));
	TestTrue(TEXT("Map tile count stays bounded"), Keys.Num() > 0 && Keys.Num() <= 1089);
	for (const FVoxelMapTileKey& Key : Keys)
	{
		TestEqual(TEXT("All visible tiles share one LOD"), Key.Step, 128);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingViewPredecessorCompletionTest,
	"WHFramework.Voxel.Streaming.View.PredecessorCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingViewPredecessorCompletionTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const TArray<FVoxelViewAdmission> Admissions = {
		{EVoxelViewAdmissionKind::Fine, 64.0},
		{EVoxelViewAdmissionKind::VoxelProxy, 128.0},
		{EVoxelViewAdmissionKind::Surface, 256.0},
		{EVoxelViewAdmissionKind::Macro, 512.0}
	};
	for (int32 CompletedKinds = 0; CompletedKinds <= 4; ++CompletedKinds)
	{
		const int32 Active = FVoxelViewManager::ResolveActiveAdmissionKind(
			Admissions,
			[CompletedKinds](const FVoxelViewAdmission& Admission)
			{
				return static_cast<int32>(Admission.Kind) < CompletedKinds;
			});
		TestEqual(TEXT("Every distant layer waits for its predecessor"),
			Active, CompletedKinds);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingMultiSourceStageTest,
	"WHFramework.Voxel.Streaming.View.MultiSourceStage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingMultiSourceStageTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TArray<FVoxelViewAdmission> Admissions = {
		{ EVoxelViewAdmissionKind::Fine, 16.0, 0 },
		{ EVoxelViewAdmissionKind::Fine, 24.0, 0 },
		{ EVoxelViewAdmissionKind::VoxelProxy, 64.0, 0 },
		{ EVoxelViewAdmissionKind::VoxelProxy, 72.0, 0 },
		{ EVoxelViewAdmissionKind::Surface, 128.0, 0 },
		{ EVoxelViewAdmissionKind::Macro, 256.0, 0 },
		{ EVoxelViewAdmissionKind::Fine, 8.0, 100 }
	};
	FVoxelViewManager::SortAdmissionsByPriority(Admissions);
	int32 Priority = MAX_int32;
	TestEqual(TEXT("Two player sources start at fine stage"),
		FVoxelViewManager::ResolveActiveAdmissionKind(Admissions,
			[](const FVoxelViewAdmission&) { return false; }, &Priority), 0);
	TestEqual(TEXT("Player sources share numeric priority"), Priority, 0);
	TestEqual(TEXT("Both player fine regions complete before player proxy"),
		FVoxelViewManager::ResolveActiveAdmissionKind(Admissions,
			[](const FVoxelViewAdmission& Admission)
			{
				return Admission.Priority == 0 && Admission.Kind == EVoxelViewAdmissionKind::Fine;
			}, &Priority), 1);
	TestEqual(TEXT("Player far layers do not wait for lower-priority debug fine"),
		FVoxelViewManager::ResolveActiveAdmissionKind(Admissions,
			[](const FVoxelViewAdmission& Admission) { return Admission.Priority == 0; }, &Priority), 0);
	TestEqual(TEXT("Remaining source has its own numeric priority"), Priority, 100);
	const TMap<int32, uint8> DataLanes = FVoxelViewManager::ResolveAdmissionLanes(Admissions,
		[](const FVoxelViewAdmission& Admission)
		{
			return Admission.Priority == 0 && Admission.Kind == EVoxelViewAdmissionKind::Fine;
		});
	const TMap<int32, uint8> MeshLanes = FVoxelViewManager::ResolveAdmissionLanes(Admissions,
		[](const FVoxelViewAdmission&) { return false; });
	TestEqual(TEXT("All distant data lanes prepare while fine meshes are unfinished"), DataLanes.FindRef(0), static_cast<uint8>(14));
	TestEqual(TEXT("Distant meshes use spare admission without a whole-layer barrier"), MeshLanes.FindRef(0), static_cast<uint8>(15));
	TestTrue(TEXT("Debug fine remains independently eligible while player proxy data prepares"),
		DataLanes.FindRef(100) == 1 && MeshLanes.FindRef(100) == 1);
	const auto CriticalLanes = FVoxelViewManager::ResolveAdmissionLanes(Admissions,
		[](const FVoxelViewAdmission&) { return false; }, true);
	TestEqual(TEXT("Movement critical fine suppresses distant admissions"), CriticalLanes.FindRef(0), static_cast<uint8>(1));
	TestEqual(TEXT("Critical protection retains each source fine lane"), CriticalLanes.FindRef(100), static_cast<uint8>(1));
	TestEqual(TEXT("All source groups finish"),
		FVoxelViewManager::ResolveActiveAdmissionKind(Admissions,
			[](const FVoxelViewAdmission&) { return true; }, &Priority), 4);
	FVoxelTaskRequest PlayerMacro;
	PlayerMacro.Kind = EVoxelTaskKind::BuildMacro;
	PlayerMacro.SourcePriority = 0;
	FVoxelTaskRequest DebugFine;
	DebugFine.Kind = EVoxelTaskKind::BuildFineMesh;
	DebugFine.SourcePriority = 100;
	TestTrue(TEXT("Higher-priority player macro outranks lower-priority debug fine"),
		FVoxelTaskScheduler::IsHigherPriority(PlayerMacro, DebugFine));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelMapTileBuildTest,
	"WHFramework.Voxel.Map.BackgroundTileBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMapTileBuildTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelGenerationRuntimeConfig MapConfig = *VoxelTest::MakeGenerationConfig();
	FVoxelGenerationRecipe MapRecipe = *MapConfig.Recipe;
	// 此用例验收后台栅格和纹理缓存；正式 Profile 的 20 km 地图另作运行验收。
	MapRecipe.Settings.HydrologyRegionSide = 16;
	MapRecipe.Settings.HydrologyHaloCells = 4;
	MapConfig.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(MapRecipe));
	const auto Config = MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(MapConfig));
	const auto Generator = MakeShared<const FVoxelGenerationPipeline, ESPMode::ThreadSafe>(
		Config, MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>());
	FVoxelTaskScheduler Scheduler;
	FVoxelMapTileCache Cache(Scheduler, Generator, Config, 1, 25.0);
	const FVoxelMapTileKey Key { FIntPoint(-1, 0), 16 };
	const FVoxelMapTileKey WorldKey { FIntPoint(0, 0), 512 };
	TArray<FVoxelMapTileKey> Requested;
	Requested.Add(Key);
	Requested.Add(WorldKey);
	FSceneMapView View;
	const double StartSeconds = FPlatformTime::Seconds();
	Cache.Request(Requested, View);
	const double Deadline = FPlatformTime::Seconds() + 90.0;
	while ((!Cache.FindBrush(Key) || !Cache.FindBrush(WorldKey)) &&
		FPlatformTime::Seconds() < Deadline)
	{
		Scheduler.Tick([](FVoxelTaskResult&&) {}, 8.0);
		FPlatformProcess::Sleep(0.01f);
	}
	const FSlateBrush* Brush = Cache.FindBrush(Key);
	TestNotNull(TEXT("Natural tile reaches the texture cache"), Brush);
	TestNotNull(TEXT("20 km world map tile reaches the texture cache"),
		Cache.FindBrush(WorldKey));
	AddInfo(FString::Printf(TEXT("Both real raster tiles prepared in %.2fs"),
		FPlatformTime::Seconds() - StartSeconds));
	if (Brush)
	{
		const UTexture2D* Texture = Cast<UTexture2D>(Brush->GetResourceObject());
		TestNotNull(TEXT("Tile has an actual texture"), Texture);
		if (Texture)
		{
			TestEqual(TEXT("Tile texture width"), Texture->GetSizeX(), FVoxelMapTileCache::TileSide);
			TestEqual(TEXT("Tile texture height"), Texture->GetSizeY(), FVoxelMapTileCache::TileSide);
		}
	}
	Scheduler.StopAndJoin();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCriticalFineDependencyClosureTest,
	"WHFramework.Voxel.Streaming.CriticalFineDependencyClosure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCriticalFineDependencyClosureTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelWorldManifest Manifest;
	Manifest.Settings.MinZ = -128;
	Manifest.Settings.MaxZ = 128;
	FVoxelViewSettings Settings;
	Settings.MacroRadiusCells = 0;
	FVoxelStreamingSource Source;
	Source.Id = FGuid::NewGuid();
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::FineVisual;
	Source.View.FineRadiusCells = 80;
	Source.View.FineVerticalRadiusCells = 64;
	Source.View.FinePreloadCells = 16;
	Source.View.FinePredictionSeconds = 2.0;
	Source.View.MovementCriticalFineRadiusCells = 16;
	Source.View.VoxelProxyRadiusCells = 0;
	Source.View.SurfaceRadiusCells = 0;
	Source.VelocityCellsPerSecond = FVector(32, 0, 0);
	FVoxelInterestRuntime Runtime;
	FVoxelInterestSet Interest;
	FVoxelInterestDelta Delta;
	bool bSnapshot = false;
	Runtime.Update(MakeArrayView(&Source, 1), Manifest, Settings, Interest, Delta, bSnapshot);
	const FVoxelExactDemand* Current = Interest.Exact.Find(FIntVector::ZeroValue);
	if (!TestNotNull(TEXT("Current observer section remains in Fine"), Current)) return false;
	TestTrue(TEXT("Prediction keeps current Fine critical even when the predicted center is far away"), Current->bMovementCriticalFine);
	int32 Critical = 0;
	for (const auto& Pair : Interest.Exact)
	{
		if (!Pair.Value.bMovementCriticalFine) continue;
		++Critical;
		TestTrue(TEXT("Critical Fine includes its own exact data"), Pair.Value.bMovementCriticalData);
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (const int32 Sign : {-1, 1})
			{
				FIntVector Neighbor = Pair.Key;
				Neighbor[Axis] += Sign;
				if (const auto* Demand = Interest.Exact.Find(Neighbor))
					TestTrue(TEXT("Every declared meshing neighbor receives critical data priority"), Demand->bMovementCriticalData);
			}
		}
	}
	TestTrue(TEXT("Fixture includes movement-critical Fine"), Critical > 0);
	bool bSeenNonCritical = false;
	for (const FIntVector& Key : Interest.ExactOrder)
	{
		const auto& Demand = Interest.Exact.FindChecked(Key);
		if (!Demand.bMovementCriticalData) bSeenNonCritical = true;
		else TestFalse(TEXT("Critical data enters admission before noncritical data of the same source"), bSeenNonCritical);
	}
	return true;
}

#endif
