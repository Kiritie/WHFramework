#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelViewLodTest, "WHFramework.Voxel.Rendering.ScreenErrorLOD",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelViewLodTest::RunTest(const FString& InParameters)
{
	int32 Step = 0;
	bool bFloor = false;
	FString Error;
	if (!TestTrue(TEXT("Actual sampling uses the specified projection"),
		VoxelStreamingGeometry::ResolveSampleStep(4096, 90.0, 1080, 2.0, 512, Step, bFloor, Error))) return false;
	TestEqual(TEXT("Actual sampling selects the largest eligible power of two"), Step, 8);
	TestFalse(TEXT("Eligible sample is above the resolution floor"), bFloor);
	TestTrue(TEXT("Greater distance permits coarser sampling"),
		VoxelStreamingGeometry::ResolveSampleStep(8192, 90.0, 1080, 2.0, 512, Step, bFloor, Error));
	TestEqual(TEXT("Doubling distance doubles the eligible physical step"), Step, 16);
	TestTrue(TEXT("Greater pixel density requires finer sampling"),
		VoxelStreamingGeometry::ResolveSampleStep(4096, 90.0, 2160, 2.0, 512, Step, bFloor, Error));
	TestEqual(TEXT("Doubling viewport height halves the eligible physical step"), Step, 4);
	TestTrue(TEXT("Narrower field of view requires finer sampling"),
		VoxelStreamingGeometry::ResolveSampleStep(4096, 45.0, 1080, 2.0, 512, Step, bFloor, Error));
	TestEqual(TEXT("Projection scale reflects the actual field of view"), Step, 4);
	const int32 RetainedStep = Step;
	const bool bRetainedFloor = bFloor;
	TestFalse(TEXT("Invalid projection fails before changing sampling outputs"),
		VoxelStreamingGeometry::ResolveSampleStep(4096, 180.0, 1080, 2.0, 512, Step, bFloor, Error));
	TestEqual(TEXT("Rejected projection retains the previous physical step"), Step, RetainedStep);
	TestEqual(TEXT("Rejected projection retains the previous resolution-floor status"), bFloor, bRetainedFloor);
	TestFalse(TEXT("Finite inputs whose screen-error product overflows are rejected"),
		VoxelStreamingGeometry::ResolveSampleStep(TNumericLimits<double>::Max(), 90.0, 1080,
			TNumericLimits<double>::Max(), 512, Step, bFloor, Error));
	TestFalse(TEXT("Projection overflow is rejected before choosing a sampling step"),
		VoxelStreamingGeometry::ResolveSampleStep(4096, 1e-308, 1080, 2.0, 512, Step, bFloor, Error));
	TestEqual(TEXT("Numeric overflow retains the previous physical step"), Step, RetainedStep);
	TestEqual(TEXT("Numeric overflow retains the previous resolution-floor status"), bFloor, bRetainedFloor);
	return true;
}

#endif
