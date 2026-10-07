#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "NavMesh/RecastHelpers.h"
#include "Voxel/Navigation/VoxelNavigationTypes.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelNavigationCapacityTest,
	"WHFramework.Voxel.Navigation.FrozenCapacityAndCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNavigationCapacityTest::RunTest(const FString&)
{
	FVoxelRegistrySnapshot Registry;
	Registry.Definitions.SetNum(2);
	Registry.Definitions[1].TypeId = 1;
	Registry.Definitions[1].bSolid = true;
	FVoxelShapeRegistry Shapes;
	Shapes.BuildDefaults();
	FVoxelSectionSnapshot Section;
	Section.Blocks.Init(0, 4096);
	Section.Blocks[4 + 16 * 4] = FVoxelBlockState(1, 0).Pack();
	const auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	Resources->Configure({});
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 7, {}, 1);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	if (!TestTrue(TEXT("填满暂存输出池"), Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged,
		Resources->Snapshot().Limits.StagedBytes, Blocker))) return false;
	FVoxelNavigationSurface Surface;
	Surface.Vertices.Add(FVector(17.0));
	FString Error;
	TestFalse(TEXT("输出分配前等待容量"), FVoxelNavigationSurfaceBuilder::Build(Section, nullptr,
		Registry, Shapes, 25.0, 175.0, Surface, nullptr, Resources, Consumer, &Error));
	TestTrue(TEXT("等待原因可恢复"), Error.StartsWith(TEXT("CapacityBlocked")));
	TestEqual(TEXT("失败不能覆盖当前输出"), Surface.Vertices[0], FVector(17.0));
	Blocker.Reset();
	if (!TestTrue(TEXT("容量释放后完整构建"), FVoxelNavigationSurfaceBuilder::Build(Section, nullptr,
		Registry, Shapes, 25.0, 175.0, Surface, nullptr, Resources, Consumer, &Error))) return false;
	TestEqual(TEXT("冻结计数与构建一致"), Surface.Vertices.Num(), 4);
	TestEqual(TEXT("冻结计数输出完整索引"), Surface.Indices.Num(), 6);
	const uint64 ActualBytes = Surface.Vertices.GetAllocatedSize() + Surface.Indices.GetAllocatedSize();
	TestTrue(TEXT("暂存租约覆盖实际容器容量"), Surface.ResourceLease && Surface.ResourceLease->Bytes() >= ActualBytes);
	TAtomic<bool> Canceled(true);
	TestFalse(TEXT("取消不能替换当前输出"), FVoxelNavigationSurfaceBuilder::Build(Section, nullptr,
		Registry, Shapes, 25.0, 175.0, Surface, &Canceled, Resources, Consumer, &Error));
	TestEqual(TEXT("取消后已完成输出保留"), Surface.Indices.Num(), 6);
	TestEqual(TEXT("取消未额外占用暂存容量"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Staged)],
		Surface.ResourceLease->Bytes());
	return true;
}
#endif
