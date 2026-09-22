#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Voxel/Generation/VoxelNaturalGenerationCache.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Generation/VoxelFeaturePlan.h"
#include "Voxel/Generation/VoxelStructurePlan.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"
#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelWarmupDemandDefaultsTest,
	"WHFramework.Voxel.Performance.WarmupDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelWarmupDemandDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FVoxelViewSettings Settings;
	TestTrue(TEXT("Warmup data is smaller than default fine radius"), Settings.WarmupDataRadius < Settings.FineRadius);
	TestTrue(TEXT("Warmup collision is smaller than default fine radius"), Settings.WarmupCollisionRadius < Settings.FineRadius);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelGenerationInfluenceBoundsTest,
	"WHFramework.Voxel.Performance.PlanInfluenceBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelGenerationInfluenceBoundsTest::RunTest(const FString& Parameters)
{
	const FVoxelGenerationBounds Neighbor { FIntVector(-16, 0, 0), FIntVector(0, 16, 16) };
	const FVoxelGenerationBounds Far { FIntVector(64), FIntVector(80) };
	FVoxelFeaturePlan Feature;
	Feature.Bounds = { FIntVector(0), FIntVector(16) };
	Feature.Writes.Add({ FIntVector(-1, 2, 3), 1, EVoxelGenerationStage::Vegetation, {} });
	Feature.Finalize();
	TestTrue(TEXT("Cross-owner feature writes are not culled"), Feature.AffectsBounds(Neighbor));
	TestFalse(TEXT("Unrelated feature plans can be skipped"), Feature.AffectsBounds(Far));
	FVoxelStructurePlan Structure;
	Structure.Clears.Add({ Neighbor, EVoxelGenerationStage::SurfaceStructures, {} });
	Structure.Finalize();
	TestTrue(TEXT("Clear-only structures retain influence"), Structure.AffectsBounds(Neighbor));
	TestFalse(TEXT("Clear-only structures do not affect distant sections"), Structure.AffectsBounds(Far));
	FVoxelEcologyPlan Ecology;
	Ecology.Writes.Add({ FIntVector(-1, 2, 3), 2, 1, {} });
	Ecology.Finalize();
	TestTrue(TEXT("Tree crowns and grass outside ownership bounds remain visible"), Ecology.AffectsBounds(Neighbor));
	Ecology.Writes.Reset();
	Ecology.Finalize();
	TestFalse(TEXT("Rebuilt empty plan clears old influence"), Ecology.AffectsBounds(Neighbor));
	FVoxelCavePlan Cave;
	Cave.Segments.Add({ FIntVector(1, 2, 3), FIntVector(4, 2, 3), 3 });
	Cave.Finalize();
	TestTrue(TEXT("Capsule radius is part of cave influence"), Cave.AffectsBounds(Neighbor));
	TestFalse(TEXT("Unrelated cave systems can be skipped"), Cave.AffectsBounds(Far));
	return true;
}

#endif
