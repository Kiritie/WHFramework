#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Ability/Inventory/InventoryItemTypes.h"
#include "Ability/Inventory/InventoryItemAssetCatalog.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/WorldObject/VoxelWorldObjectDefinition.h"
#include "Voxel/WorldObject/VoxelWorldObjectRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelInventoryPlacementDeclarationTest,
    "WHFramework.Voxel.Inventory.ExplicitPlacementDeclaration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelInventoryPlacementDeclarationTest::RunTest(const FString& Parameters)
{
    UVoxelData* Block = NewObject<UVoxelData>();
    Block->BlockName = TEXT("test:ordinary_block");
    const FInventoryItemDefinition Undeclared = Block->BuildInventoryDefinition();
    TestFalse(TEXT("A registered block is not implicitly placeable"), Undeclared.PlaceableVoxelId.IsValid());

    Block->UsePolicy = EInventoryUsePolicy::PlaceVoxel;
    UInventoryItemAssetCatalog* Catalog = NewObject<UInventoryItemAssetCatalog>();
    TArray<UAbilityItemDataBase*> Assets{Block};
    TArray<FInventoryItemDefinition> Definitions;
    FString Error;
    if (!TestTrue(TEXT("A declared block passes the actual item catalog"), Catalog->Initialize(Assets, Definitions, Error)))
    {
        AddError(Error);
        return false;
    }
    if (!TestEqual(TEXT("The catalog contains the declared item"), Definitions.Num(), 1)) return false;
    TestEqual(TEXT("The placement target is the same voxel asset"), Definitions[0].PlaceableVoxelId, Block->GetPrimaryAssetId());

    FInventoryItemRegistry Validator;
    Block->bRegisterBlock = false;
    TestFalse(TEXT("Unregistered blocks cannot declare placement"), Validator.ValidateDefinition(Block->BuildInventoryDefinition(), Error));
    Block->bRegisterBlock = true;
    Block->BlockName = TEXT("core:air");
    TestFalse(TEXT("Air cannot declare placement"), Validator.ValidateDefinition(Block->BuildInventoryDefinition(), Error));
    Block->BlockName = NAME_None;
    TestFalse(TEXT("A block without a stable name cannot declare placement"), Validator.ValidateDefinition(Block->BuildInventoryDefinition(), Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelInventoryObjectAnchorTest,
    "WHFramework.Voxel.Inventory.ObjectAnchorPlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelInventoryObjectAnchorTest::RunTest(const FString& Parameters)
{
    UVoxelData* Anchor = NewObject<UVoxelData>();
    Anchor->BlockName = TEXT("test:object_anchor");
    Anchor->Shape = EVoxelShapeKind::ObjectPart;
    Anchor->UsePolicy = EInventoryUsePolicy::PlaceVoxel;
    UVoxelData* Part = NewObject<UVoxelData>();
    Part->BlockName = TEXT("test:object_part");
    Part->Shape = EVoxelShapeKind::ObjectPart;
    UVoxelWorldObjectDefinition* Object = NewObject<UVoxelWorldObjectDefinition>();
    Object->DefinitionId = TEXT("Test.Object");
    Object->Footprint.AddDefaulted_GetRef().BlockName = Anchor->BlockName;
    FVoxelWorldObjectPart& Tail = Object->Footprint.AddDefaulted_GetRef();
    Tail.BlockName = Part->BlockName;
    Tail.Offset = FIntVector(1, 0, 0);
    TArray<UVoxelData*> Blocks{Anchor, Part};
    TArray<UVoxelWorldObjectDefinition*> Objects{Object};
    FString Error;
    TestTrue(TEXT("Declared root and non-placeable part are accepted"),
        FVoxelWorldObjectRegistry::ValidatePlaceableObjectItems(Blocks, Objects, Error));
    Part->UsePolicy = EInventoryUsePolicy::PlaceVoxel;
    TestFalse(TEXT("A declared internal part is rejected"),
        FVoxelWorldObjectRegistry::ValidatePlaceableObjectItems(Blocks, Objects, Error));
    Part->UsePolicy = EInventoryUsePolicy::None;
    Anchor->bRegisterBlock = false;
    TestFalse(TEXT("An unregistered root is rejected"),
        FVoxelWorldObjectRegistry::ValidatePlaceableObjectItems(Blocks, Objects, Error));
    Anchor->bRegisterBlock = true;
    Object->Footprint[0].Offset = FIntVector(1, 0, 0);
    TestFalse(TEXT("A root with a nonzero origin is rejected"),
        FVoxelWorldObjectRegistry::ValidatePlaceableObjectItems(Blocks, Objects, Error));
    return true;
}

#endif
