#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Voxel/Rendering/VoxelCoverage.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelCoverageTest,
	"WHFramework.Voxel.Rendering.Coverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoverageTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const FVoxelCoverageRect Parent2D { FIntPoint(-32, -32), FIntPoint(32, 32) };
	TArray<FVoxelCoverageRect> Children2D {
		{ FIntPoint(-32, -32), FIntPoint(0, 0) },
		{ FIntPoint(0, -32), FIntPoint(32, 0) },
		{ FIntPoint(-32, 0), FIntPoint(0, 32) }
	};
	TestFalse(TEXT("Missing 2D quadrant is not full coverage"), VoxelCoverage::IsFullyCovered2D(Parent2D, Children2D));
	Children2D.Add({ FIntPoint(0, 0), FIntPoint(32, 32) });
	TestTrue(TEXT("Four 2D quadrants cover parent"), VoxelCoverage::IsFullyCovered2D(Parent2D, Children2D));
	const FVoxelCoverageRect NegativeTarget { FIntPoint(-128, -96), FIntPoint(-64, -32) };
	const TArray<FVoxelCoverageRect> NegativeCoverage {
		{ FIntPoint(-128, -96), FIntPoint(-96, -32) },
		{ FIntPoint(-96, -96), FIntPoint(-64, -32) }
	};
	TestTrue(TEXT("Negative coordinates cover correctly"),
		VoxelCoverage::IsFullyCovered2D(NegativeTarget, NegativeCoverage));

	const FVoxelCoverageBox Parent3D { FIntVector(-16), FIntVector(16) };
	TArray<FVoxelCoverageBox> Children3D;
	for (int32 Z = 0; Z < 2; ++Z)
	{
		for (int32 Y = 0; Y < 2; ++Y)
		{
			for (int32 X = 0; X < 2; ++X)
			{
				const FIntVector Min(-16 + X * 16, -16 + Y * 16, -16 + Z * 16);
				Children3D.Add({ Min, Min + FIntVector(16) });
			}
		}
	}
	TestTrue(TEXT("Eight 3D children cover parent"), VoxelCoverage::IsFullyCovered3D(Parent3D, Children3D));
	Children3D.Pop();
	TestFalse(TEXT("Missing 3D octant is not full coverage"), VoxelCoverage::IsFullyCovered3D(Parent3D, Children3D));
	const TArray<FVoxelCoverageBox> WrongHeight {
		{ FIntVector(-16, -16, -48), FIntVector(16, 16, -16) }
	};
	TestFalse(TEXT("Matching XY at wrong Z cannot replace volume"), VoxelCoverage::IsFullyCovered3D(Parent3D, WrongHeight));

	FVoxelVoxelProxyData Proxy;
	Proxy.GridSide = 16;
	Proxy.Cells.Init(FVoxelBlockState(), 4096);
	TestFalse(TEXT("All-air proxy has no top surface evidence"), VoxelCoverage::HasProxyTopSurfaceInColumn(Proxy, 3, 5));
	Proxy.Cells[3 + 5 * 16 + 15 * 256] = FVoxelBlockState { 1, 0 };
	Proxy.Known[4] = true;
	Proxy.Halo[4].Init(FVoxelBlockState(), 256);
	TestTrue(TEXT("Solid top cell with +Z air halo is surface evidence"), VoxelCoverage::HasProxyTopSurfaceInColumn(Proxy, 3, 5));
	Proxy.Known[4] = false;
	TestFalse(TEXT("Unknown +Z halo is conservatively not surface evidence"), VoxelCoverage::HasProxyTopSurfaceInColumn(Proxy, 3, 5));

	return true;
}

#endif
