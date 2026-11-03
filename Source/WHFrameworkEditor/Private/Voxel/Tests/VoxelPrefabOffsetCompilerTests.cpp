#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Voxel/Authoring/VoxelStructureData.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelStructure.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/VoxelGenerationCompiler.h"

struct FVoxelPrefabCompilerTestAccess
{
	static bool Compile(const UVoxelStructureData& InStructure, const FVoxelRegistrySnapshot& InRegistry,
		const TMap<FName, uint16>& InSymbols, int32 InTargetSize, FVoxelStructurePieceTemplate& OutPiece, FString& OutError)
	{
		return FVoxelGenerationCompiler::CompileStructurePiece(InStructure, InRegistry, InSymbols, InTargetSize, OutPiece, OutError);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPrefabStructureCompilerOffsetTest,
	"WHFramework.Voxel.Prefab.StructureCompilerOffset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPrefabStructureCompilerOffsetTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	const FPrimaryAssetId AssetId(TEXT("Voxel:PrefabCompilerTest"));
	FVoxelRegistrySnapshot Registry;
	Registry.Definitions.SetNum(2);
	Registry.Definitions[1].TypeId = 1;
	Registry.Definitions[1].BlockName = TEXT("test:prefab_compiler_block");
	Registry.Definitions[1].AssetID = AssetId;
	Registry.Assets.Add(AssetId, 1);
	Registry.Names.Add(Registry.Definitions[1].BlockName, 1);
	TMap<FName, uint16> Symbols;
	Symbols.Add(Registry.Definitions[1].BlockName, 1);

	TStrongObjectPtr<UVoxelPrefabData> Prefab(NewObject<UVoxelPrefabData>());
	Prefab->CenterOffset = FVector(3, -2, 4);
	FVoxelPrefabCell& Block = Prefab->Data.Cells.AddDefaulted_GetRef();
	Block.Offset = FIntVector(1, 2, -3);
	Block.Item = FVoxelItem(AssetId);
	FVoxelPrefabCell& Clear = Prefab->Data.Cells.AddDefaulted_GetRef();
	Clear.Offset = FIntVector(0, 1, 0);
	Clear.bClear = true;
	TStrongObjectPtr<UVoxelStructureData> Structure(NewObject<UVoxelStructureData>());
	Structure->StableId = TEXT("test:prefab_structure");
	Structure->SourcePrefab = Prefab.Get();
	Structure->SourceCellCentimeters = 50;
	Structure->SourceBounds.Min = FIntVector(-2, -3, -4);
	Structure->SourceBounds.Max = FIntVector(6, 7, 8);
	Structure->SourceEntranceCorner = FIntVector(2, 0, 0);
	FVoxelGridBox& ClearVolume = Structure->ClearVolumes.AddDefaulted_GetRef();
	ClearVolume.Min = FIntVector(2, 2, 2);
	ClearVolume.Max = FIntVector(3, 3, 3);
	FVoxelStructureConnectorData& Connector = Structure->Connectors.AddDefaulted_GetRef();
	Connector.Type = TEXT("test:connector");
	Connector.Position = FIntVector(1, 1, 1);

	FString Error;
	FVoxelStructurePieceTemplate Raw;
	if (!TestTrue(TEXT("Raw structure compiles"), FVoxelPrefabCompilerTestAccess::Compile(*Structure, Registry, Symbols, 25, Raw, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("The unshifted entrance remains the placement reference"), Raw.Entrance, FIntVector(4, 0, 0));
	TestEqual(TEXT("Ground height follows the prefab offset and scale"), Raw.GroundZ, 8);
	TestEqual(TEXT("Bounds minimum follows the prefab offset and scale"), Raw.Bounds.Min, FIntVector(2, -10, 0));
	TestEqual(TEXT("Bounds maximum follows the prefab offset and scale"), Raw.Bounds.Max, FIntVector(18, 10, 24));
	TestEqual(TEXT("Connector follows the prefab offset and scale"), Raw.Connectors[0].Position, FIntVector(8, -2, 10));
	TestTrue(TEXT("Solid write follows the prefab offset and scale"), Raw.Writes.ContainsByPredicate([](const FVoxelStructureRun& Run)
	{
		return Run.Start == FIntVector(8, 0, 2) && Run.Length == 2 && Run.Value == 1;
	}));
	TestTrue(TEXT("Explicit source clear volume follows the offset"), Raw.ClearVolumes.ContainsByPredicate([](const FVoxelGenerationBounds& Bounds)
	{
		return Bounds.Min == FIntVector(10, 0, 12) && Bounds.Max == FIntVector(12, 2, 14);
	}));
	TestTrue(TEXT("Prefab clear cell is represented as a protected clear volume"), Raw.ClearVolumes.ContainsByPredicate([](const FVoxelGenerationBounds& Bounds)
	{
		return Bounds.Min == FIntVector(6, -2, 8) && Bounds.Max == FIntVector(8, -1, 9);
	}));

	const FVoxelPrefabSaveData Source = Prefab->Data;
	if (!TestTrue(TEXT("Source packs"), Prefab->SetPackedData(Source, Error))) return false;
	FVoxelStructurePieceTemplate Packed;
	if (!TestTrue(TEXT("Packed structure compiles"), FVoxelPrefabCompilerTestAccess::Compile(*Structure, Registry, Symbols, 25, Packed, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Packed and raw write counts agree"), Packed.Writes.Num(), Raw.Writes.Num());
	TestEqual(TEXT("Packed and raw clear counts agree"), Packed.ClearVolumes.Num(), Raw.ClearVolumes.Num());
	TestEqual(TEXT("Packed and raw bounds agree"), Packed.Bounds.Min, Raw.Bounds.Min);
	TestEqual(TEXT("Packed and raw entrance agree"), Packed.Entrance, Raw.Entrance);
	TestTrue(TEXT("Packed structure contains the same shifted solid"), Packed.Writes.ContainsByPredicate([](const FVoxelStructureRun& Run)
	{
		return Run.Start == FIntVector(8, 0, 2) && Run.Length == 2 && Run.Value == 1;
	}));

	Prefab->CenterOffset = FVector::ZeroVector;
	FVoxelStructurePieceTemplate Unshifted;
	if (!TestTrue(TEXT("Unshifted structure compiles"), FVoxelPrefabCompilerTestAccess::Compile(*Structure, Registry, Symbols, 25, Unshifted, Error))) return false;
	FVoxelGenerationRecipe Recipe;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FVoxelStructureRuntimeDefinition Definition;
		Definition.StableId = Index ? TEXT("test:compiled_shifted") : TEXT("test:compiled_original");
		Definition.StableHash = Index + 1;
		Definition.Stage = EVoxelGenerationStage::SurfaceStructures;
		Definition.Pieces.Add(Index ? Raw : Unshifted);
		Recipe.StructureLookup.Add(Definition.StableId, Recipe.Structures.Num());
		Recipe.Structures.Add(MoveTemp(Definition));
	}
	const FVoxelStructurePlanner Planner(MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe)));
	const FIntVector ExpectedOffsets[] = {FIntVector(6, -4, 8), FIntVector(4, 6, 8),
		FIntVector(-6, 4, 8), FIntVector(-4, -6, 8)};
	for (uint8 Yaw = 0; Yaw < 4; ++Yaw)
	{
		FVoxelPlannedStructurePlacement Request;
		Request.Anchor = FIntVector(100, -40, 20);
		Request.Id = VoxelGeneration::MakeStableId(173, Request.Anchor, 29);
		Request.Yaw = Yaw;
		Request.DefinitionId = TEXT("test:compiled_original");
		FVoxelStructureInstance Before;
		if (!TestTrue(TEXT("Original compiled structure rasterizes"), Planner.BuildAt(Request, Before, Error))) return false;
		Request.DefinitionId = TEXT("test:compiled_shifted");
		FVoxelStructureInstance After;
		if (!TestTrue(TEXT("Shifted compiled structure rasterizes"), Planner.BuildAt(Request, After, Error))) return false;
		if (!TestTrue(TEXT("Original rasterization has writes and clears"), !Before.Writes.IsEmpty() && !Before.ClearVolumes.IsEmpty()) ||
			!TestTrue(TEXT("Shifted rasterization has writes and clears"), !After.Writes.IsEmpty() && !After.ClearVolumes.IsEmpty())) return false;
		TestEqual(TEXT("Compiler offset survives entrance anchoring and rotation"),
			After.Writes[0].Position - Before.Writes[0].Position, ExpectedOffsets[Yaw]);
		TestEqual(TEXT("Compiled clear volume moves with the structure"),
			After.ClearVolumes[0].Bounds.Min - Before.ClearVolumes[0].Bounds.Min, ExpectedOffsets[Yaw]);
	}
	return true;
}

#endif
