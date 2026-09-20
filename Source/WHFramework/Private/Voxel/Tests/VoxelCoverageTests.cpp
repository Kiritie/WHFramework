#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Voxel/Rendering/VoxelCoverage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelCoverageTest,
	"WHFramework.Voxel.Rendering.Coverage",
	EAutomationTestFlags::
		EditorContext |
	EAutomationTestFlags::
		EngineFilter)

bool FVoxelCoverageTest::RunTest(
	const FString& InParameters)
{
	(void)InParameters;

	const FVoxelCoverageRect Parent {
		FIntPoint(
			-32,
			-32),
		FIntPoint(
			32,
			32)
	};

	const TArray<FVoxelCoverageRect> ThreeChildren {
		{
			FIntPoint(-32, -32),
			FIntPoint(0, 0)
		},
		{
			FIntPoint(0, -32),
			FIntPoint(32, 0)
		},
		{
			FIntPoint(-32, 0),
			FIntPoint(0, 32)
		}
	};

	TestFalse(
		TEXT(
			"Three of four quadrants do not cover parent"),
		VoxelCoverage::
			IsFullyCovered(
				Parent,
				ThreeChildren));

	TArray<FVoxelCoverageRect> AllChildren =
		ThreeChildren;

	AllChildren.Add({
		FIntPoint(0, 0),
		FIntPoint(32, 32)
	});

	TestTrue(
		TEXT(
			"Four quadrants cover parent"),
		VoxelCoverage::
			IsFullyCovered(
				Parent,
				AllChildren));

	const FVoxelCoverageRect Negative {
		FIntPoint(
			-128,
			-96),
		FIntPoint(
			-64,
			-32)
	};

	const TArray<FVoxelCoverageRect> NegativeCoverage {
		{
			FIntPoint(-128, -96),
			FIntPoint(-96, -32)
		},
		{
			FIntPoint(-96, -96),
			FIntPoint(-64, -32)
		}
	};

	TestTrue(
		TEXT(
			"Negative coordinates cover correctly"),
		VoxelCoverage::
			IsFullyCovered(
				Negative,
				NegativeCoverage));

	return true;
}

#endif
