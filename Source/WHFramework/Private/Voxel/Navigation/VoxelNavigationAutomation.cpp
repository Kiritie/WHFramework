#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "NavMesh/RecastHelpers.h"
#include "Voxel/Navigation/VoxelNavigationTypes.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Runtime/VoxelRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNavigationClearanceTest,
	"WHFramework.Voxel.Navigation.CrossSectionClearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNavigationClearanceTest::RunTest(const FString&)
{
	FVoxelRegistrySnapshot Registry;
	Registry.Definitions.SetNum(2);
	Registry.Definitions[1].TypeId = 1;
	Registry.Definitions[1].bSolid = true;
	FVoxelShapeRegistry Shapes;
	Shapes.BuildDefaults();
	FVoxelSectionSnapshot Lower, Above;
	Lower.Blocks.Init(0,4096);
	Above.Blocks.Init(0,4096);
	Lower.Blocks[4+16*(4+16*15)] = FVoxelBlockState{1,0}.Pack();
	FVoxelNavigationSurface Surface;
	TestTrue(TEXT("Missing upper snapshot is a conservative build"),
		FVoxelNavigationSurfaceBuilder::Build(Lower,nullptr,Registry,Shapes,25,175,Surface));
	TestTrue(TEXT("Missing clearance never creates a navigable face"),Surface.Indices.IsEmpty());
	TestTrue(TEXT("Clear upper section can publish the support face"),
		FVoxelNavigationSurfaceBuilder::Build(Lower,&Above,Registry,Shapes,25,175,Surface));
	TestEqual(TEXT("One walkable top face"),Surface.Indices.Num(),6);
	for (int32 Index = 0; Index + 2 < Surface.Indices.Num(); Index += 3)
	{
		const FVector A = Unreal2RecastPoint(Surface.Vertices[Surface.Indices[Index]]);
		const FVector B = Unreal2RecastPoint(Surface.Vertices[Surface.Indices[Index + 1]]);
		const FVector C = Unreal2RecastPoint(Surface.Vertices[Surface.Indices[Index + 2]]);
		TestTrue(TEXT("Top faces remain upward and walkable after the engine's Recast coordinate conversion"),
			FVector::CrossProduct(B - A, C - A).GetSafeNormal().Y > 0.999);
	}
	Above.Blocks[4+16*(4+16*5)] = FVoxelBlockState{1,0}.Pack();
	FVoxelNavigationSurfaceBuilder::Build(Lower,&Above,Registry,Shapes,25,175,Surface);
	TestTrue(TEXT("Low ceiling in adjacent section rejects the face"),Surface.Indices.IsEmpty());
	TAtomic<bool> Cancelled(true);
	TestFalse(TEXT("Cancelled jobs cannot publish partial geometry"),
		FVoxelNavigationSurfaceBuilder::Build(Lower,&Above,Registry,Shapes,25,175,Surface,&Cancelled));
	return true;
}
#endif
