#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Hydrology/VoxelHydrology.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelWaterfallPlanTest,"WHFramework.Voxel.Hydrology.StableWaterfallSemantic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelWaterfallPlanTest::RunTest(const FString&)
{
	FVoxelHydrologyPlan Plan;
	Plan.CoreMin=FIntPoint(0,0);
	Plan.CoreMax=FIntPoint(64,64);
	Plan.Grid.CellSize=4;
	Plan.RiverMeanderStrength=0;
	FVoxelRiverRoute& River=Plan.Rivers.AddDefaulted_GetRef();
	FVoxelRiverRoutePoint Up,Down;
	Up.Position=FIntPoint(8,8); Up.WaterZ=40; Up.WaterHalfWidth=4;
	Down.Position=FIntPoint(20,8); Down.WaterZ=20; Down.WaterHalfWidth=4;
	River.Points={Up,Down};
	Plan.Finalize();
	if (!TestTrue(TEXT("A steep canonical river reach produces waterfall semantics"),!Plan.Waterfalls.IsEmpty())) return false;
	const auto First=Plan.Waterfalls;
	Plan.Finalize();
	TestEqual(TEXT("Re-finalizing does not duplicate falls"),First.Num(),Plan.Waterfalls.Num());
	for (int32 Index=0; Index<FMath::Min(First.Num(),Plan.Waterfalls.Num()); ++Index)
	{
		TestTrue(TEXT("Fall identity is stable"),First[Index].Id==Plan.Waterfalls[Index].Id);
		TestTrue(TEXT("Downstream water remains lower"),Plan.Waterfalls[Index].Upstream.Z>Plan.Waterfalls[Index].Downstream.Z);
	}
	Plan.Rivers[0].Points[1].WaterZ=40;
	Plan.Finalize();
	TestTrue(TEXT("A level river does not fabricate a waterfall"),Plan.Waterfalls.IsEmpty());
	return true;
}
#endif
