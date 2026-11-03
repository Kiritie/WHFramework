#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelStructure.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Tests/VoxelTestUtilities.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPrefabPlacementOffsetTest,
	"WHFramework.Voxel.Prefab.PlacementOffset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPrefabPlacementOffsetTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TStrongObjectPtr<UVoxelPrefabData> Prefab(NewObject<UVoxelPrefabData>());
	FVoxelPrefabSaveData Raw;
	FVoxelPrefabCell& Block = Raw.Cells.AddDefaulted_GetRef();
	Block.Offset = FIntVector(4, 2, -2);
	Block.Item = FVoxelItem(FPrimaryAssetId(TEXT("Voxel:PrefabOffsetTest")));
	FVoxelPrefabCell& Clear = Raw.Cells.AddDefaulted_GetRef();
	Clear.Offset = FIntVector(-1, 0, 1);
	Clear.bClear = true;
	Prefab->Data = Raw;
	Prefab->CenterOffset = FVector(2, -3, 3);
	FString Error;
	FVoxelPrefabSaveData Placement;
	if (!TestTrue(TEXT("Raw asset decodes placement"), Prefab->DecodePlacementCells(Placement, Error))) return false;
	TestEqual(TEXT("Solid offset follows all three axes"), Placement.Cells[0].Offset, FIntVector(6, -1, 1));
	TestEqual(TEXT("Clear offset follows all three axes"), Placement.Cells[1].Offset, FIntVector(1, -3, 4));
	TestTrue(TEXT("Clear marker is retained"), Placement.Cells[1].bClear);
	FVoxelPrefabSaveData Original;
	TestTrue(TEXT("Raw decode remains available"), Prefab->DecodeCells(Original, Error));
	TestEqual(TEXT("Raw coordinates remain unchanged"), Original.Cells[0].Offset, FIntVector(4, 2, -2));
	const FBox Bounds = Prefab->GetVoxelBounds();
	TestTrue(TEXT("Placement bounds include both solid and clear cells"), Bounds.IsValid != 0);
	TestEqual(TEXT("Placement bounds minimum"), Bounds.Min, FVector(1, -3, 1));
	TestEqual(TEXT("Placement bounds maximum is exclusive"), Bounds.Max, FVector(7, 0, 5));

	if (!TestTrue(TEXT("Raw prefab packs"), Prefab->SetPackedData(Raw, Error))) return false;
	TestEqual(TEXT("Packing preserves the asset offset"), Prefab->CenterOffset, FVector(2, -3, 3));
	FVoxelPrefabSaveData PackedPlacement;
	if (!TestTrue(TEXT("Packed asset decodes placement"), Prefab->DecodePlacementCells(PackedPlacement, Error))) return false;
	TestEqual(TEXT("Packed and raw solid coordinates agree"), PackedPlacement.Cells[0].Offset, Placement.Cells[0].Offset);
	TestEqual(TEXT("Packed and raw clear coordinates agree"), PackedPlacement.Cells[1].Offset, Placement.Cells[1].Offset);
	TestTrue(TEXT("Packed clear marker is retained"), PackedPlacement.Cells[1].bClear);
	FVoxelPrefabSaveData Again;
	TestTrue(TEXT("Repeated decode succeeds"), Prefab->DecodePlacementCells(Again, Error));
	TestEqual(TEXT("Repeated decode does not apply offset twice"), Again.Cells[0].Offset, Placement.Cells[0].Offset);

	Prefab->CenterOffset = FVector(0, 0, 0.5);
	TestFalse(TEXT("Fractional cell offsets are rejected"), Prefab->DecodePlacementCells(Again, Error));
	TestFalse(TEXT("Fractional offset has no placement bounds"), Prefab->GetVoxelBounds().IsValid != 0);
	Prefab->CenterOffset = FVector(0, 0, std::numeric_limits<double>::quiet_NaN());
	TestFalse(TEXT("Non-finite offsets are rejected"), Prefab->DecodePlacementCells(Again, Error));
	Prefab->CenterOffset = FVector(VoxelBlock::MaxAbsCoordinate, 0, 0);
	TestFalse(TEXT("Out-of-bounds translated cells are rejected"), Prefab->DecodePlacementCells(Again, Error));

	Prefab->CenterOffset = FVector::ZeroVector;
	TestTrue(TEXT("Default offset decodes"), Prefab->DecodePlacementCells(Again, Error));
	TestEqual(TEXT("Default offset retains legacy geometry"), Again.Cells[0].Offset, Raw.Cells[0].Offset);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPrefabPlacementRotationTest,
	"WHFramework.Voxel.Prefab.PlacementRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPrefabPlacementRotationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	TStrongObjectPtr<UVoxelPrefabData> Prefab(NewObject<UVoxelPrefabData>());
	FVoxelPrefabCell& Block = Prefab->Data.Cells.AddDefaulted_GetRef();
	Block.Offset = FIntVector(4, 2, -2);
	Block.Item = FVoxelItem(FPrimaryAssetId(TEXT("Voxel:PrefabOffsetTest")));
	FString Error;
	FVoxelPrefabSaveData Original;
	FVoxelPrefabSaveData Shifted;
	if (!TestTrue(TEXT("Original cells decode"), Prefab->DecodePlacementCells(Original, Error))) return false;
	Prefab->CenterOffset = FVector(2, -3, 3);
	if (!TestTrue(TEXT("Shifted cells decode"), Prefab->DecodePlacementCells(Shifted, Error))) return false;

	FVoxelGenerationRecipe Recipe = *VoxelTest::MakeGenerationConfig()->Recipe;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FIntVector Cell = Index ? Shifted.Cells[0].Offset : Original.Cells[0].Offset;
		FVoxelStructureRuntimeDefinition Definition;
		Definition.StableId = Index ? TEXT("test:prefab_shifted") : TEXT("test:prefab_original");
		Definition.StableHash = Index + 1;
		Definition.Stage = EVoxelGenerationStage::SurfaceStructures;
		FVoxelStructurePieceTemplate Piece;
		Piece.StableId = Definition.StableId;
		Piece.Bounds = {Cell, Cell + FIntVector(1)};
		Piece.Writes.Add({Cell, 1, 1});
		Piece.ClearVolumes.Add(Piece.Bounds);
		Definition.Pieces.Add(Piece);
		Recipe.Structures.Add(MoveTemp(Definition));
	}
	if (!TestTrue(TEXT("Rotation recipe builds"), Recipe.BuildLookups(Error))) return false;
	const FVoxelStructurePlanner Planner(MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe)));
	const FIntVector ExpectedOffsets[] = {FIntVector(2, -3, 3), FIntVector(3, 2, 3),
		FIntVector(-2, 3, 3), FIntVector(-3, -2, 3)};
	for (uint8 Yaw = 0; Yaw < 4; ++Yaw)
	{
		FVoxelPlannedStructurePlacement Request;
		Request.Anchor = FIntVector(50, -20, 17);
		Request.Id = VoxelGeneration::MakeStableId(173, Request.Anchor, 29);
		Request.Yaw = Yaw;
		Request.DefinitionId = TEXT("test:prefab_original");
		FVoxelStructureInstance Before;
		if (!TestTrue(TEXT("Original structure rasterizes"), Planner.BuildAt(Request, Before, Error))) return false;
		Request.DefinitionId = TEXT("test:prefab_shifted");
		FVoxelStructureInstance After;
		if (!TestTrue(TEXT("Shifted structure rasterizes"), Planner.BuildAt(Request, After, Error))) return false;
		if (!TestEqual(TEXT("Both placements contain one write"), After.Writes.Num(), 1) ||
			!TestEqual(TEXT("Original placement contains one write"), Before.Writes.Num(), 1) ||
			!TestEqual(TEXT("Both placements contain one clear"), After.ClearVolumes.Num(), 1) ||
			!TestEqual(TEXT("Original placement contains one clear"), Before.ClearVolumes.Num(), 1)) return false;
		TestEqual(TEXT("Real rasterizer rotates the prefab offset"),
			After.Writes[0].Position - Before.Writes[0].Position, ExpectedOffsets[Yaw]);
		TestEqual(TEXT("Clear area rotates with geometry"),
			After.ClearVolumes[0].Bounds.Min - Before.ClearVolumes[0].Bounds.Min, ExpectedOffsets[Yaw]);
	}
	return true;
}

#endif
