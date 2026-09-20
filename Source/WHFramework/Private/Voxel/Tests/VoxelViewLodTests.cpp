#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Voxel/Rendering/VoxelMacroTerrain.h"
#include "Voxel/Rendering/VoxelViewLod.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelViewLodTest,
	"WHFramework.Voxel.Rendering.ScreenErrorLOD",
	EAutomationTestFlags::
		EditorContext |
	EAutomationTestFlags::
		EngineFilter)

bool FVoxelViewLodTest::RunTest(
	const FString& InParameters)
{
	(void)InParameters;

	FVoxelStreamingSource Source;

	Source.VerticalFovDegrees =
		90.0f;

	Source.ViewportHeightPixels =
		1080;

	FVoxelViewSettings Settings;

	Settings.TargetScreenErrorPixels =
		2.0f;

	const int32 Distance =
		4096;

	const uint8 SurfaceLevel =
		VoxelViewLod::
			ResolveScreenErrorLevel(
				Distance,
				1,
				Source,
				Settings,
				4);

	const uint8 MacroLevel =
		VoxelViewLod::
			ResolveScreenErrorLevel(
				Distance,
				FVoxelMacroTileData::
					BaseStep,
				Source,
				Settings,
				4);

	TestTrue(
		TEXT(
			"Macro base step produces equal-or-finer LOD than surface at same distance"),
		MacroLevel <=
			SurfaceLevel);

	TestTrue(
		TEXT(
			"Macro does not ignore BaseStep"),
		MacroLevel !=
			SurfaceLevel ||
		MacroLevel == 0);

	return true;
}

#endif
