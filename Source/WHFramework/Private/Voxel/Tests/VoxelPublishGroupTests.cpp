#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelPublishGroups.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelLocalPublishGroupTest,
	"WHFramework.Voxel.Rendering.LocalPublishGroups",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelLocalPublishGroupTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	auto Box = [](const double X0, const double Y0, const double X1, const double Y1)
	{
		return FBox(FVector(X0, Y0, 0), FVector(X1, Y1, 1));
	};
	TArray<FVoxelPublishFootprint> Footprints = {
		{{3, {0, 0}, 0}, Box(0, 0, 64, 64), false},
		{{2, {0, 0}, 0}, Box(0, 0, 32, 32), false},
		{{2, {1, 0}, 0}, Box(32, 0, 64, 32), false},
		{{3, {1, 0}, 0}, Box(64, 0, 128, 64), false},
		{{4, {0, 0}, 0}, Box(64, 0, 64, 16), true},
		{{3, {4, 0}, 0}, Box(256, 0, 320, 64), false},
		{{2, {8, 0}, 0}, FBox(ForceInit), false}
	};
	TArray<int32> Groups;
	FVoxelPublishGroupPlanner::Build(Footprints, Groups);
	TestEqual(TEXT("Every actor receives a group"), Groups.Num(), Footprints.Num());
	if (Groups.Num() != Footprints.Num()) return false;
	TestEqual(TEXT("Parent and both children commit together"), Groups[0], Groups[1]);
	TestEqual(TEXT("Sibling children share the parent handoff"), Groups[1], Groups[2]);
	TestEqual(TEXT("Transition joins both sides of a boundary"), Groups[0], Groups[3]);
	TestEqual(TEXT("Transition belongs to its seam group"), Groups[0], Groups[4]);
	TestNotEqual(TEXT("Distant terrain publishes independently"), Groups[0], Groups[5]);
	TestNotEqual(TEXT("Empty source has no spatial dependency"), Groups[0], Groups[6]);
	Footprints.RemoveAt(4);
	FVoxelPublishGroupPlanner::Build(Footprints, Groups);
	TestNotEqual(TEXT("Touching tiles without a transition remain independent"), Groups[0], Groups[3]);
	const FBox FineBounds = FVoxelPublishGroupPlanner::OwnershipBounds({0, {-1, 2}, 0}, 100.0);
	TestTrue(TEXT("Fine ownership bounds are valid"), FineBounds.IsValid != 0);
	TestEqual(TEXT("Negative fine coordinate starts at the correct world position"), FineBounds.Min.X, -1600.0);
	TestEqual(TEXT("Fine ownership width is sixteen cells"), FineBounds.Max.X, 0.0);
	const FBox SurfaceBounds = FVoxelPublishGroupPlanner::OwnershipBounds({2, {2, -1}, 1}, 50.0);
	TestEqual(TEXT("Surface level scales ownership width"), SurfaceBounds.Max.X - SurfaceBounds.Min.X, 3200.0);
	TestEqual(TEXT("Surface negative coordinate is aligned"), SurfaceBounds.Min.Y, -3200.0);
	const FBox MacroBounds = FVoxelPublishGroupPlanner::OwnershipBounds({3, {0, 0}, 0}, 100.0);
	TestEqual(TEXT("Macro owns 2048 cells"), MacroBounds.Max.X, 204800.0);
	const FBox ProxyBounds = FVoxelPublishGroupPlanner::OwnershipBounds({1, {0, 0}, 1}, 100.0);
	TestEqual(TEXT("Volume proxy owner keeps its full footprint after a transition rebuild"),
		ProxyBounds.Max.X, 3200.0);
	TestFalse(TEXT("Unsupported representation has no ownership"),
		FVoxelPublishGroupPlanner::OwnershipBounds({9, {0, 0}, 0}, 100.0).IsValid != 0);
	TestFalse(TEXT("Invalid block size has no ownership"),
		FVoxelPublishGroupPlanner::OwnershipBounds({2, {0, 0}, 0}, 0.0).IsValid != 0);
	TArray<FVoxelPublishFootprint> AdjacentTransitions = {
		{{2, {0, 0}, 0}, Box(0, 0, 32, 32), false},
		{{2, {1, 0}, 0}, Box(32, 0, 64, 32), false},
		{{2, {2, 0}, 0}, Box(64, 0, 96, 32), false},
		{{4, {0, 0}, 0}, Box(0, 0, 0, 16), true},
		{{4, {2, 0}, 0}, Box(96, 0, 96, 16), true},
		{{4, {1, 0}, 0}, FBox(ForceInit), true}
	};
	FVoxelPublishGroupPlanner::Build(AdjacentTransitions, Groups);
	TestEqual(TEXT("Empty seam still commits with its owner"), Groups[1], Groups[5]);
	TestNotEqual(TEXT("Distant seams do not chain all neighboring tiles"), Groups[0], Groups[2]);
	TArray<FVoxelPublishFootprint> VolumeReplacement = {
		{{1, {0, 0}, 1}, FVoxelPublishGroupPlanner::OwnershipBounds({1, {0, 0}, 1}, 100.0), false},
		{{0, {0, 0}, 0}, FVoxelPublishGroupPlanner::OwnershipBounds({0, {0, 0}, 0}, 100.0), false},
		{{1, {1, 0}, 1}, FVoxelPublishGroupPlanner::OwnershipBounds({1, {1, 0}, 1}, 100.0), false}
	};
	FVoxelPublishGroupPlanner::Build(VolumeReplacement, Groups);
	TestEqual(TEXT("Rebuilt proxy and fine child publish together"), Groups[0], Groups[1]);
	TestNotEqual(TEXT("Adjacent proxy remains independent"), Groups[0], Groups[2]);
	return true;
}

#endif
