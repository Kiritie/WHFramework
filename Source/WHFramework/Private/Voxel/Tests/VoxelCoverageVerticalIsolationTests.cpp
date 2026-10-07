#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCoverageVerticalIsolationTest,
	"WHFramework.Voxel.Rendering.CoverageVerticalIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelCoverageVerticalIsolationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const auto MakeRecord = [](const EVoxelTerrainRepresentation Representation, const FIntVector& Coordinate,
		const uint8 Level, const int32 Step, const int32 MinZ = -32, const int32 MaxZ = 64)
	{
		auto Record = MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>();
		Record->Geometry = {{Representation, Coordinate, Level}, Step, MinZ, MaxZ, 17, 23};
		Record->Geometry.Key.TryBounds(MinZ, MaxZ, Record->Min, Record->Max);
		return Record;
	};
	FString Error;
	FVoxelCoverageIndex Index;
	const auto Ground = MakeRecord(EVoxelTerrainRepresentation::Fine, {}, 0, 1);
	if (!TestTrue(TEXT("Ground atom is indexed"), Index.Upsert(Ground, Error))) return false;
	FVoxelCoverageLocalSnapshot Before;
	Index.Capture({}, FIntVector(16), Before);
	TestEqual(TEXT("Local query sees one ground atom"), Before.Records.Num(), 1);
	const auto Elevated = MakeRecord(EVoxelTerrainRepresentation::VoxelProxy, FIntVector(0, 0, 1024), 0, 2);
	if (!TestTrue(TEXT("Proxy at the same XY and distant Z is indexed"), Index.Upsert(Elevated, Error))) return false;
	TestTrue(TEXT("Adding a vertically distant owner preserves the ground read guard"), Index.IsCurrent(Before));
	FVoxelCoverageLocalSnapshot After;
	Index.Capture({}, FIntVector(16), After);
	TestEqual(TEXT("Vertical query avoids the distant record"), After.ExaminedRecords, 1);
	TestEqual(TEXT("Vertical query retains only ground coverage"), After.Records.Num(), 1);
	const auto LocalProxy = MakeRecord(EVoxelTerrainRepresentation::VoxelProxy, {}, 0, 2);
	if (!TestTrue(TEXT("A local ownership alternative is indexed"), Index.Upsert(LocalProxy, Error))) return false;
	TestFalse(TEXT("Local membership changes invalidate the captured read guard"), Index.IsCurrent(After));

	FVoxelCoverageIndex Columns;
	const auto Surface = MakeRecord(EVoxelTerrainRepresentation::Surface, {}, 1, 2);
	if (!TestTrue(TEXT("Heightfield column is indexed"), Columns.Upsert(Surface, Error))) return false;
	FVoxelCoverageLocalSnapshot Below, Above;
	Columns.Capture(FIntVector(0, 0, -16), FIntVector(16, 16, 0), Below);
	Columns.Capture(FIntVector(0, 0, 80), FIntVector(16, 16, 96), Above);
	TestEqual(TEXT("Heightfield remains available across its declared world height column"), Below.Records.Num(), 1);
	TestTrue(TEXT("Heightfield is absent outside its actual vertical bounds"), Above.Records.IsEmpty());
	return true;
}

#endif
