#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelSurfaceProxy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelCapsuleContinuityTest,
	"WHFramework.Voxel.Generation.Cave.CapsuleContinuity",
	EAutomationTestFlags::EditorContext |
	EAutomationTestFlags::EngineFilter)

bool FVoxelCapsuleContinuityTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;

	const FIntVector Start(
		0,
		0,
		10);

	const FIntVector End(
		10,
		0,
		10);

	for (int32 X = 0;
		X <= 10;
		++X)
	{
		TestTrue(
			FString::Printf(
				TEXT("Segment must carve continuously at X=%d"),
				X),
			VoxelGeneration::IsInsideCapsule(
				FIntVector(
					X,
					0,
					10),
				Start,
				End,
				2));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelHeightfieldVertexContractTest,
	"WHFramework.Voxel.Rendering.Heightfield.VertexContract",
	EAutomationTestFlags::EditorContext |
	EAutomationTestFlags::EngineFilter)

bool FVoxelHeightfieldVertexContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;

	TestEqual(
		TEXT("Surface vertex side"),
		FVoxelSurfaceTileData::
			VertexSide,
		33);

	TestEqual(
		TEXT("Surface cell side"),
		FVoxelSurfaceTileData::
			CellSide,
		32);

	TestEqual(
		TEXT("Macro vertex side"),
		FVoxelMacroTileData::
			VertexSide,
		33);

	TestEqual(
		TEXT("Macro cell side"),
		FVoxelMacroTileData::
			CellSide,
		32);

	return true;
}

#endif
