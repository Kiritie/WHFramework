#include "Misc/AutomationTest.h"
#include "Voxel/Geometry/DWVoxelBoundaryTransition.h"
#include "Voxel/Rendering/VoxelFineShardBuild.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFineBoundarySourceLifetimeTest,
	"WHFramework.Voxel.Rendering.BoundarySourceLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelFineBoundarySourceLifetimeTest::RunTest(const FString&)
{
	TSharedPtr<FVoxelFineShardGeometry, ESPMode::ThreadSafe> Geometry = MakeShared<FVoxelFineShardGeometry, ESPMode::ThreadSafe>();
	TSharedPtr<FVoxelFineBoundarySnapshot, ESPMode::ThreadSafe> Boundary = MakeShared<FVoxelFineBoundarySnapshot, ESPMode::ThreadSafe>();
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> Mesh = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
	Geometry->GeometryIdentity = FGuid::NewGuid();
	Geometry->Attempt.Input.Shard = {FIntVector::ZeroValue, 16};
	Geometry->Attempt.Input.DataRevision = 7;
	Boundary->Input = Geometry->Attempt.Input;
	auto& Atom = Boundary->Atoms.AddDefaulted_GetRef();
	Atom.Section = FIntVector::ZeroValue;
	Atom.Owned[0].Add(0);
	Geometry->Boundary = Boundary;
	Geometry->Pieces.Add({Geometry->Attempt.Input.Shard, Mesh});
	const TWeakPtr<FVoxelFineShardGeometry, ESPMode::ThreadSafe> WeakGeometry = Geometry;
	const TWeakPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> WeakMesh = Mesh;
	const TWeakPtr<FVoxelFineBoundarySnapshot, ESPMode::ThreadSafe> WeakBoundary = Boundary;
	FVoxelBoundaryFaceSnapshot Face;
	if (!TestTrue(TEXT("Capture exact production boundary source"), FVoxelBoundaryFaceSnapshot::CaptureFineShardAtom(
		{FIntVector::ZeroValue, 0}, 0, Geometry, Face))) return false;
	TestTrue(TEXT("Current geometry matches"), Face.FineSource.Matches(*Geometry));
	const FGuid OriginalIdentity = Geometry->GeometryIdentity;
	Geometry->GeometryIdentity = FGuid::NewGuid();
	TestFalse(TEXT("A new geometry identity is stale even with the same boundary"), Face.FineSource.Matches(*Geometry));
	Geometry->GeometryIdentity = OriginalIdentity;
	++Geometry->Attempt.Input.DataRevision;
	TestFalse(TEXT("Changed content cannot reuse the captured proof"), Face.FineSource.Matches(*Geometry));
	--Geometry->Attempt.Input.DataRevision;
	Geometry.Reset();
	Mesh.Reset();
	Boundary.Reset();
	TestFalse(TEXT("Boundary proof does not retain the geometry owner"), WeakGeometry.IsValid());
	TestFalse(TEXT("Consumed CPU mesh is released"), WeakMesh.IsValid());
	TestTrue(TEXT("Frozen samples remain alive"), WeakBoundary.IsValid());
	uint32 Packed = MAX_uint32;
	TestTrue(TEXT("First frozen sample survives geometry retirement"), Face.GetPackedState(0, Packed));
	TestEqual(TEXT("First exact packed sample"), Packed, 0u);
	TestTrue(TEXT("Last frozen sample survives geometry retirement"), Face.GetPackedState(255, Packed));
	Face = {};
	TestFalse(TEXT("Final boundary consumer releases its samples"), WeakBoundary.IsValid());
	return true;
}

#endif
