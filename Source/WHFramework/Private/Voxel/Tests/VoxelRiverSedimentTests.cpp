#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Generation/Hydrology/VoxelHydrology.h"
#include "Voxel/Generation/Surface/VoxelSurfaceGenerator.h"
#include "Voxel/Generation/Surface/VoxelRiverSediment.h"

namespace
{
    FVoxelGenerationRecipe SedimentRecipe()
    {
        FVoxelGenerationRecipe Recipe;
        Recipe.Settings.Seed = 114;
        Recipe.Palette.Stone = 1;
        Recipe.Palette.Dirt = 2;
        Recipe.Palette.Grass = 3;
        Recipe.Palette.Sand = 4;
        Recipe.Palette.Snow = 5;
        Recipe.Biomes.AddDefaulted_GetRef().DefaultSurface = 3;
        Recipe.Biomes[0].SurfaceRuleIndex = 0;
        FVoxelSurfaceRuntimeRuleSet& Rules = Recipe.SurfaceRules.AddDefaulted_GetRef();
        FVoxelSurfaceRuntimeRule& Grass = Rules.Rules.AddDefaulted_GetRef();
        Grass.MinDepth = 0;
        Grass.MaxDepth = 0;
        Grass.BlockSymbol = 3;
        FVoxelSurfaceRuntimeRule& Dirt = Rules.Rules.AddDefaulted_GetRef();
        Dirt.MinDepth = 1;
        Dirt.MaxDepth = 3;
        Dirt.BlockSymbol = 2;
        return Recipe;
    }

    FVoxelSurfaceGenerator SedimentSurface(FVoxelGenerationRecipe Recipe)
    {
        return FVoxelSurfaceGenerator(
            MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(Recipe)));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelRiverSedimentPlanChainTest,
    "WHFramework.Voxel.Hydrology.Sediment.RealPlanChain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelRiverSedimentPlanChainTest::RunTest(const FString& InParameters)
{
    (void)InParameters;
    FVoxelHydrologyPlan Plan;
    Plan.Grid.WorldMinCell = FIntPoint(-72, -4);
    Plan.Grid.CellSize = 32;
    Plan.Grid.Width = 144;
    Plan.Grid.Height = 8;
    Plan.CoreMin = FIntPoint(-2304, -128);
    Plan.CoreMax = FIntPoint(2304, 128);
    Plan.RiverMeanderStrength = 0;
    Plan.RiverSeed = 114;
    FVoxelRiverRoute& Route = Plan.Rivers.AddDefaulted_GetRef();
    Route.Id.High = 1;
    Route.Id.Low = 2;
    for (const int32 X : {-2208, 2208})
    {
        FVoxelRiverRoutePoint& Point = Route.Points.AddDefaulted_GetRef();
        Point.Position = FIntPoint(X, 0);
        Point.WaterZ = 20;
        Point.BedHalfWidth = 4;
        Point.WaterHalfWidth = 8;
        Point.BankWidth = 8;
        Point.ShoreWidth = 8;
        Point.Depth = 4;
    }
    FString Error;
    if (!TestTrue(TEXT("Actual plan finalizes"), Plan.Finalize(Error))) return false;
    const FVoxelSurfaceGenerator Surface = SedimentSurface(SedimentRecipe());
    FVoxelGenerationRecipe DisabledRecipe = SedimentRecipe();
    DisabledRecipe.Palette.Sand = MAX_uint16;
    const FVoxelSurfaceGenerator Disabled = SedimentSurface(MoveTemp(DisabledRecipe));
    int32 Sand = 0, Grass = 0, GapRun = 0, LongestGap = 0;
    TSet<int32> Widths;
    FIntVector SandCell = FIntVector::ZeroValue;
    FVoxelColumnSample SandColumn;
    for (int32 X = -2048; X < 2048; ++X)
    {
        int32 Width = 0;
        for (int32 Y = 9; Y <= 16; ++Y)
        {
            FVoxelHydrologyInfluence Influence;
            if (!TestTrue(TEXT("Plan samples a real dry bank"), Plan.Sample(X, Y, 22, Influence))) return false;
            TestTrue(TEXT("Fixture genuinely lies in DryBank"), Influence.RiverZone == EVoxelRiverSurfaceZone::DryBank);
            TestEqual(TEXT("Original BankDistance semantics remain zero in DryBank"), Influence.BankDistanceCells, 0);
            FVoxelColumnSample Column;
            Column.SurfaceZ = Column.DensityHeight = 22;
            Column.BiomeIndex = 0;

            VoxelHydrology::ApplyInfluenceToColumn(Influence, Column);
            Column.DensityHeight = Column.SurfaceZ;
            FVoxelColumnSample Off = Column;
            Surface.ResolveColumn(X, Y, Column);
            Disabled.ResolveColumn(X, Y, Off);
            TestEqual(TEXT("Sediment cannot change ground"), Column.SurfaceZ, Off.SurfaceZ);
            TestEqual(TEXT("Sediment cannot change density"), Column.DensityHeight, Off.DensityHeight);
            TestEqual(TEXT("Sediment cannot change water"), Column.SurfaceWaterZ, Off.SurfaceWaterZ);
            TestTrue(TEXT("Sediment cannot change wet zone"), Column.RiverZone == Off.RiverZone && Column.bRiver == Off.bRiver);
            TestEqual(TEXT("Fine and distant use identical material resolution"),
                Surface.ResolveSymbol(FIntVector(X, Y, Column.SurfaceZ), Column, 0), uint32(Column.SurfaceMaterial));
            if (Column.SurfaceMaterial == 4)
            {
                ++Sand; ++Width;
                SandCell = FIntVector(X, Y, Column.SurfaceZ);
                SandColumn = Column;
            }
            Grass += Column.SurfaceMaterial == 3 ? 1 : 0;
        }
        Widths.Add(Width);
        GapRun = Width == 0 ? GapRun + 1 : 0;
        LongestGap = FMath::Max(LongestGap, GapRun);
    }
    TestTrue(TEXT("Actual bank contains sand and original grass"), Sand > 0 && Grass > 0);
    TestTrue(TEXT("Whole cross-bank gaps last at least eight cells"), LongestGap >= 8);
    TestTrue(TEXT("Patch widths genuinely vary"), Widths.Num() >= 4);
    if (Sand == 0) return false;

    for (const uint16 Protected : {uint16(1), uint16(5), uint16(17)})
    {
        FVoxelGenerationRecipe Recipe = SedimentRecipe();
        Recipe.SurfaceRules[0].Rules[0].BlockSymbol = Protected;
        TestEqual(TEXT("Ordinary stone/snow/special material is preserved at a sand-positive cell"),
            SedimentSurface(MoveTemp(Recipe)).ResolveSymbol(SandCell, SandColumn, 0), uint32(Protected));
    }
    FVoxelGenerationRecipe Explicit = SedimentRecipe();
    Explicit.SurfaceRules[0].Rules[0].RiverZone = EVoxelSurfaceRiverRule::DryBank;
    Explicit.SurfaceRules[0].Rules[0].BlockSymbol = 17;
    TestEqual(TEXT("Explicit RiverZone rule wins"),
        SedimentSurface(MoveTemp(Explicit)).ResolveSymbol(SandCell, SandColumn, 0), uint32(17));
    FVoxelGenerationRecipe Special = SedimentRecipe();
    Special.SurfaceRules[0].Rules[0].bCoastOnly = true;
    SandColumn.bCoast = true;
    TestEqual(TEXT("Special coastal grass is preserved"),
        SedimentSurface(MoveTemp(Special)).ResolveSymbol(SandCell, SandColumn, 0), uint32(3));
    SandColumn.bCoast = false;
    TestEqual(TEXT("Only the surface layer can change"), Surface.ResolveSymbol(SandCell, SandColumn, 1), uint32(2));
    FVoxelHydrologyInfluence Wet;
    TestTrue(TEXT("Actual wet channel samples"), Plan.Sample(0, 0, 22, Wet));
    FVoxelColumnSample Bed;
    Bed.SurfaceZ = 22;
    Bed.BiomeIndex = 0;
    VoxelHydrology::ApplyInfluenceToColumn(Wet, Bed);
    TestEqual(TEXT("Actual wet bed keeps subsoil"), Surface.ResolveSymbol(FIntVector(0, 0, Bed.SurfaceZ), Bed, 0), uint32(2));
    return true;
}

#endif
