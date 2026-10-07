#include "Voxel/Rendering/VoxelTerrainCoarseMesh.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	using FGeometryPtr = TSharedPtr<const FVoxelTerrainCoarseMesh, ESPMode::ThreadSafe>;
	using FPatchPtr = TSharedPtr<const FVoxelTerrainCoarsePatch, ESPMode::ThreadSafe>;

	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> MakeSource()
	{
		auto Source = MakeShared<FVoxelSectionMeshResult, ESPMode::ThreadSafe>();
		Source->Stamp.WorldEpoch = 1;
		Source->Stamp.OwnerId = FGuid::NewGuid();
		Source->Stamp.OwnerIncarnation = Source->Stamp.Token = 1;
		Source->Stamp.bHasGeometryIdentity = true;
		Source->Stamp.GeometryIdentity.Key = {EVoxelTerrainRepresentation::Surface, FIntVector::ZeroValue, 20};
		Source->Stamp.GeometryIdentity.SampleStepCells = 1024;
		auto& Mesh = Source->Batches.AddDefaulted_GetRef().Mesh;
		for (int32 Triangle = 0; Triangle < 64; ++Triangle)
		{
			const FVector Origin = Triangle < 32 ? FVector::ZeroVector : FVector(1000000, 0, 0);
			for (const FVector& Vertex : {Origin, Origin + FVector(16, 0, 0), Origin + FVector(0, 16, 0)})
			{
				Mesh.Triangles.Add(Mesh.Vertices.Num());
				Mesh.Vertices.Add(Vertex);
				Mesh.Normals.Add(FVector::UpVector);
				Mesh.UV0.Add(FVector2D(Vertex.X, Vertex.Y));
				Mesh.UV1.Add(FVector2D(Vertex.X * 2, Vertex.Y * 2));
				Mesh.UV2.Add(FVector2D(Vertex.X * 3, Vertex.Y * 3));
				Mesh.Colors.Add(FLinearColor::White);
				Mesh.Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
			}
		}
		Source->ValidateAndCache();
		return Source;
	}

	FGeometryPtr BuildGeometry(const TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe>& Resources,
		const FVoxelGenerationConsumerKey& Consumer, const TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe>& Source)
	{
		FVoxelTerrainCoarseMeshBuilder Builder(Resources, Consumer);
		FString Error;
		if (!Builder.Initialize(Source, FVector::ZeroVector, 1.0, Source->Stamp, Error)) return nullptr;
		TAtomic<bool> Cancel{false};
		for (int32 Slice = 0; Slice < 1024; ++Slice)
		{
			const auto Progress = Builder.Advance(1, Cancel, Error);
			if (Progress == EVoxelTerrainCoarseMeshProgress::Complete) return Builder.TakeResult();
			if (Progress != EVoxelTerrainCoarseMeshProgress::Working) return nullptr;
		}
		return nullptr;
	}

	FPatchPtr BuildPatch(const TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe>& Resources,
		const FVoxelGenerationConsumerKey& Consumer, const FGeometryPtr& Geometry, const FPatchPtr& Base,
		TConstArrayView<FBox> Boxes, TConstArrayView<FBox> Changes)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::SharedBytes<FVoxelTerrainCoarseExclusions>() + VoxelBoundedAllocation::ArrayBytes<FBox>(Boxes.Num()), Lease)) return nullptr;
		auto Exclusions = MakeShared<FVoxelTerrainCoarseExclusions, ESPMode::ThreadSafe>();
		Exclusions->Lease = MoveTemp(Lease);
		Exclusions->Boxes.Reserve(Boxes.Num());
		Exclusions->Boxes.Append(Boxes.GetData(), Boxes.Num());
		FVoxelTerrainCoarsePatchBuilder Builder(Resources, Consumer);
		FString Error;
		if (!Builder.Initialize(Geometry, Base, Exclusions, Changes, Error)) return nullptr;
		TAtomic<bool> Cancel{false};
		for (int32 Slice = 0; Slice < 20000; ++Slice)
		{
			const auto Progress = Builder.Advance(1, Cancel, Error);
			if (Progress == EVoxelTerrainCoarseMeshProgress::Complete) return Builder.TakeResult();
			if (Progress != EVoxelTerrainCoarseMeshProgress::Working) return nullptr;
		}
		return nullptr;
	}

	double PieceArea(const TSharedPtr<const FVoxelTerrainCoarsePatchPiece, ESPMode::ThreadSafe>& First)
	{
		double Area = 0;
		for (auto Piece = First; Piece; Piece = Piece->Next)
		{
			const auto& Mesh = Piece->Mesh->Batches[0].Mesh;
			for (int32 Index = 0; Index < Mesh.Triangles.Num(); Index += 3)
				Area += FVector::CrossProduct(Mesh.Vertices[Mesh.Triangles[Index + 1]] - Mesh.Vertices[Mesh.Triangles[Index]],
					Mesh.Vertices[Mesh.Triangles[Index + 2]] - Mesh.Vertices[Mesh.Triangles[Index]]).Size() * 0.5;
		}
		return Area;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainCoarseRangeSharingTest,
	"WHFramework.Voxel.FineRefactor.Rendering.CoarseRangeSharing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainCoarseRangeSharingTest::RunTest(const FString& Parameters)
{
	auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	auto Source = MakeSource();
	const auto Geometry = BuildGeometry(Resources, Consumer, Source);
	if (!TestTrue(TEXT("共享范围构建完成"), Geometry.IsValid())) return false;
	TestTrue(TEXT("保留原始几何身份及引用"), Geometry->Source == Source && Geometry->Stamp == Source->Stamp);
	TestEqual(TEXT("百万格跨度仅有两个32三角形叶子及根"), Geometry->Ranges.Num(), 3);
	TestTrue(TEXT("范围元数据按实际分配收费"), Geometry->MetadataLease->Bytes() >= Geometry->GetAllocatedBytes());
	const FBox NearBox(FVector(0, 0, -1), FVector(8, 8, 1));
	const FBox FarBox(FVector(1000000, 0, -1), FVector(1000008, 8, 1));
	const auto Near = BuildPatch(Resources, Consumer, Geometry, nullptr, MakeArrayView(&NearBox, 1), MakeArrayView(&NearBox, 1));
	if (!TestTrue(TEXT("局部裁切完成"), Near && Near->Roots[0])) return false;
	const auto NearLeaf = Near->Roots[0]->Children[0];
	TestTrue(TEXT("远处叶子直接共享原始索引范围"), !Near->Roots[0]->Children[1]);
	if (!TestTrue(TEXT("近处有裁切后的叶子"), NearLeaf && NearLeaf->FirstPiece)) return false;
	TestTrue(TEXT("只扣除盒内三角形面积"), FMath::IsNearlyEqual(PieceArea(NearLeaf->FirstPiece), 32.0 * 64.0, 1.0e-6));
	for (auto Piece = NearLeaf->FirstPiece; Piece; Piece = Piece->Next)
	{
		TestTrue(TEXT("局部 Piece 有效且不超过8MiB"), Piece->Mesh->bValidated && Piece->Mesh->ResourceLease->Bytes() <= 8ull * 1024 * 1024);
		const auto& Mesh = Piece->Mesh->Batches[0].Mesh;
		for (int32 Vertex = 0; Vertex < Mesh.Vertices.Num(); ++Vertex)
		{
			const FVector2D XY(Mesh.Vertices[Vertex].X, Mesh.Vertices[Vertex].Y);
			if (!TestTrue(TEXT("插值保留三套UV及法线"), Mesh.UV0[Vertex].Equals(XY) && Mesh.UV1[Vertex].Equals(XY * 2) &&
				Mesh.UV2[Vertex].Equals(XY * 3) && Mesh.Normals[Vertex].Equals(FVector::UpVector))) return false;
		}
	}
	const FBox BothBoxes[] = {NearBox, FarBox};
	const auto Both = BuildPatch(Resources, Consumer, Geometry, Near, BothBoxes, MakeArrayView(&FarBox, 1));
	if (!TestTrue(TEXT("远处变化完成"), Both && Both->Roots[0])) return false;
	TestTrue(TEXT("近处已裁切叶子保持同一不可变引用"), Both->Roots[0]->Children[0] == NearLeaf);
	const auto Restored = BuildPatch(Resources, Consumer, Geometry, Both, MakeArrayView(&FarBox, 1), MakeArrayView(&NearBox, 1));
	if (!TestTrue(TEXT("撤销近处覆盖完成"), Restored && Restored->Roots[0])) return false;
	TestTrue(TEXT("撤销后恢复原始索引范围而非重复裁切"), !Restored->Roots[0]->Children[0]);
	TestTrue(TEXT("远处未变叶子保留引用"), Restored->Roots[0]->Children[1] == Both->Roots[0]->Children[1]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainCoarseCapacityRecoveryTest,
	"WHFramework.Voxel.FineRefactor.Rendering.CoarseCapacityRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelTerrainCoarseCapacityRecoveryTest::RunTest(const FString& Parameters)
{
	auto Resources = MakeShared<FVoxelTerrainResources, ESPMode::ThreadSafe>();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	auto Source = MakeSource();
	FVoxelTerrainCoarseMeshBuilder Builder(Resources, Consumer);
	FString Error;
	if (!TestTrue(TEXT("初始化不展开空间 Atom"), Builder.Initialize(Source, FVector::ZeroVector, 1000000.0, Source->Stamp, Error))) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Blocker;
	TestTrue(TEXT("占满Staged容量"), Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, Resources->Snapshot().Limits.StagedBytes, Blocker));
	TAtomic<bool> Cancel{false};
	TestEqual(TEXT("分配前等待容量"), Builder.Advance(1, Cancel, Error), EVoxelTerrainCoarseMeshProgress::CapacityBlocked);
	Blocker.Reset();
	EVoxelTerrainCoarseMeshProgress Progress = EVoxelTerrainCoarseMeshProgress::Working;
	for (int32 Slice = 0; Slice < 1024 && Progress == EVoxelTerrainCoarseMeshProgress::Working; ++Slice) Progress = Builder.Advance(1, Cancel, Error);
	TestEqual(TEXT("容量释放后同一构建继续完成"), Progress, EVoxelTerrainCoarseMeshProgress::Complete);
	const auto Geometry = Builder.TakeResult();
	TestTrue(TEXT("空间尺度不会扩大几何或节点数"), Geometry && Geometry->Ranges.Num() == 3 && Geometry->Source == Source);
	return true;
}
#endif
