#include "Voxel/Geometry/VoxelSectionMesher.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Materials/Material.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Rendering/VoxelCoarseBuildStorage.h"
#include "Voxel/Rendering/VoxelMeshClipper.h"

struct FVoxelAttributeCollisionValue
{
	uint64 First = 0;
	uint64 Second = 0;
};

template<>
struct TVoxelMeshAttributeTraits<FVoxelAttributeCollisionValue>
{
	static uint32 Hash(const FVoxelAttributeCollisionValue&) { return 0; }
	static bool Equal(const FVoxelAttributeCollisionValue& InFirst, const FVoxelAttributeCollisionValue& InSecond)
	{
		return InFirst.First == InSecond.First && InFirst.Second == InSecond.Second;
	}
};

namespace
{
	FVoxelSectionMeshResult MakeAttributeMesh()
	{
		FVoxelSectionMeshResult Result;
		auto& Mesh = Result.Batches.AddDefaulted_GetRef().Mesh;
		for (int32 Quad = 0; Quad < 256; ++Quad)
		{
			const int32 Base = Mesh.Vertices.Num();
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				const FVector2D UV((Corner == 1 || Corner == 2) ? 1 : 0, Corner >= 2 ? 1 : 0);
				Mesh.Vertices.Add(FVector(Quad * 2 + UV.X, UV.Y, 0));
				Mesh.Normals.Add(FVector::UpVector);
				Mesh.UV0.Add(UV);
				Mesh.UV1.Add(FVector2D(3, 4));
				Mesh.UV2.Add(FVector2D(24, 0));
				Mesh.Colors.Add(FLinearColor(0.1f, 0.2f, 0.3f, 0.75f));
				Mesh.Tangents.Add(FProcMeshTangent(FVector::ForwardVector, Corner & 1));
			}
			Mesh.Triangles.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
		}
		Result.ValidateAndCache();
		return Result;
	}

	bool SameAttributeMesh(const FVoxelSectionMeshResult& InFirst, const FVoxelSectionMeshResult& InSecond)
	{
		if (InFirst.Batches.Num() != InSecond.Batches.Num()) return false;
		for (int32 Batch = 0; Batch < InFirst.Batches.Num(); ++Batch)
		{
			const auto& First = InFirst.Batches[Batch].Mesh;
			const auto& Second = InSecond.Batches[Batch].Mesh;
			if (First.Vertices != Second.Vertices || First.Triangles != Second.Triangles || First.UV0 != Second.UV0) return false;
			for (int32 Index = 0; Index < First.Vertices.Num(); ++Index)
			{
				if (!TVoxelMeshAttributeTraits<FVector>::Equal(First.Normals[Index], Second.Normals[Index]) ||
					!TVoxelMeshAttributeTraits<FVector2D>::Equal(First.UV1[Index], Second.UV1[Index]) ||
					!TVoxelMeshAttributeTraits<FVector2D>::Equal(First.UV2[Index], Second.UV2[Index]) ||
					!TVoxelMeshAttributeTraits<FLinearColor>::Equal(First.Colors[Index], Second.Colors[Index]) ||
					!TVoxelMeshAttributeTraits<FProcMeshTangent>::Equal(First.Tangents[Index], Second.Tangents[Index])) return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMeshAttributeValuesTest,
	"WHFramework.Voxel.FineRefactor.Rendering.MeshAttributeValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMeshAttributeValuesTest::RunTest(const FString& Parameters)
{
	TVoxelMeshAttributeStream<FVector2D> Values;
	for (int32 Index = 0; Index < 4096; ++Index) Values.Add(FVector2D(Index & 1 ? -0.0 : 0.0, 1.0));
	const auto Original = Values;
	const uint64 Before = Values.GetAllocatedSize();
	TestTrue(TEXT("零预算保留原正文"), Values.Compact(0) && !Values.IsPacked() && Values.GetAllocatedSize() == Before);
	TAtomic<bool> Cancel(true);
	TestFalse(TEXT("取消不提交编码"), Values.Compact(65536, &Cancel));
	TestFalse(TEXT("取消保留原存储"), Values.IsPacked());
	TestTrue(TEXT("原值字典编码"), Values.Compact(65536) && Values.IsPacked());
	TestTrue(TEXT("实际分配下降"), Values.GetAllocatedSize() < Before);
	for (int32 Index = 0; Index < Values.Num(); ++Index)
		if (!TestTrue(TEXT("正负零及每个分量位值一致"), TVoxelMeshAttributeTraits<FVector2D>::Equal(Values[Index], Original[Index]))) return false;
	auto Copied = Values;
	auto Moved = MoveTemp(Copied);
	TVoxelMeshAttributeStream<FVector2D> Appended;
	Appended.Append(Moved);
	TestEqual(TEXT("编码源合并保持逻辑长度"), Appended.Num(), Original.Num());
	for (int32 Index = 0; Index < Appended.Num(); ++Index)
		if (!TVoxelMeshAttributeTraits<FVector2D>::Equal(Appended[Index], Original[Index])) return false;
	TVoxelMeshAttributeStream<FVoxelAttributeCollisionValue> Collision;
	for (int32 Index = 0; Index < 4096; ++Index) Collision.Add({uint64(Index % 256), uint64(Index % 256 + 17)});
	TestTrue(TEXT("256值及完全哈希冲突仍可无损编码"), Collision.Compact(65536) && Collision.IsPacked());
	for (int32 Index = 0; Index < Collision.Num(); ++Index)
		if (Collision[Index].First != uint64(Index % 256) || Collision[Index].Second != uint64(Index % 256 + 17)) return false;
	TVoxelMeshAttributeStream<FVector2D> Entropy;
	for (int32 Index = 0; Index < 4096; ++Index) Entropy.Add(FVector2D(Index % 257, Index % 257));
	TestTrue(TEXT("高熵数据保留原编码"), Entropy.Compact(65536) && !Entropy.IsPacked());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMeshAttributeConsumersTest,
	"WHFramework.Voxel.FineRefactor.Rendering.MeshAttributeConsumers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMeshAttributeConsumersTest::RunTest(const FString& Parameters)
{
	const auto Original = MakeAttributeMesh();
	auto Packed = Original;
	if (!TestTrue(TEXT("完整网格编码成功且属性保持"), Packed.CompactAttributes(65536) && SameAttributeMesh(Original, Packed))) return false;
	TestTrue(TEXT("原顶点与边界未变"), Packed.LocalBounds == Original.LocalBounds && Packed.VertexCount == Original.VertexCount);
	TestTrue(TEXT("缓存与实际存储字节一致"), Packed.Bytes() == Packed.Batches[0].Mesh.Bytes() && Packed.Bytes() < Original.Bytes());
	TArray<FBox> Boxes{FBox(FVector(2.25, -1, -1), FVector(2.75, 2, 1))};
	FVoxelSectionMeshResult RawClip;
	FVoxelSectionMeshResult PackedClip;
	VoxelMeshClipper::Subtract(Original, Boxes, RawClip);
	VoxelMeshClipper::Subtract(Packed, Boxes, PackedClip);
	TestTrue(TEXT("正式裁切的三角形和插值属性完全一致"), SameAttributeMesh(RawClip, PackedClip));
	auto* Component = NewObject<UVoxelMeshComponent>();
	if (!TestTrue(TEXT("编辑器网格读取编码源"), Component->Apply(Packed.Batches[0].Mesh, 25, UMaterial::GetDefaultMaterial(MD_Surface)))) return false;
	const auto* Section = Component->GetProcMeshSection(0);
	if (!TestNotNull(TEXT("实际生成ProcMeshSection"), Section)) return false;
	TestEqual(TEXT("目标顶点数不变"), Section->ProcVertexBuffer.Num(), Original.Batches[0].Mesh.Vertices.Num());
	for (int32 Index = 0; Index < Section->ProcVertexBuffer.Num(); ++Index)
	{
		const auto& Vertex = Section->ProcVertexBuffer[Index];
		const auto& Mesh = Original.Batches[0].Mesh;
		if (Vertex.Position != Mesh.Vertices[Index] || Vertex.Normal != Mesh.Normals[Index] || Vertex.UV1 != Mesh.UV1[Index] ||
			Vertex.UV2 != Mesh.UV2[Index] || Vertex.Color != Mesh.Colors[Index].ToFColor(false) ||
			!TVoxelMeshAttributeTraits<FProcMeshTangent>::Equal(Vertex.Tangent, Mesh.Tangents[Index])) return false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMeshAttributeCapacityTest,
	"WHFramework.Voxel.FineRefactor.Rendering.MeshAttributeCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelMeshAttributeCapacityTest::RunTest(const FString& Parameters)
{
	FVoxelTerrainResourceLimits Limits;
	Limits.InputBytes = 1024 * 1024;
	Limits.WorkingBytes = 1024 * 1024;
	Limits.StagedBytes = 2 * 1024 * 1024;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(1, FGuid::NewGuid(), 1, {}, 1);
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FString Error;
	if (!FVoxelTerrainResources::Create(Limits, Consumer, Resources, Error)) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, 4096, Metadata)) return false;
	FVoxelCoarseBuildStorage Storage(Resources.ToSharedRef(), Consumer, Metadata, nullptr);
	Storage.GeometryCapacityBytes = 1024 * 1024;
	Storage.WorkingCapacityBytes = 4096;
	FVoxelTaskResult Result;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
	if (!Storage.Begin(Result, Working)) return false;
	Result.SurfaceMesh = Storage.MakeResult<FVoxelSectionMeshResult>();
	const auto Lease = Result.SurfaceMesh->ResourceLease;
	*Result.SurfaceMesh = MakeAttributeMesh();
	Result.SurfaceMesh->ResourceLease = Lease;
	const uint64 Before = Result.SurfaceMesh->AllocatedBytes();
	const uint64 WorkingBefore = Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)];
	Result.bSuccess = true;
	if (!TestTrue(TEXT("正式完成入口编码与退款"), Storage.SettleCompleted(Result))) return false;
	TestTrue(TEXT("生产入口实际减少正文分配"), Result.SurfaceMesh->AllocatedBytes() < Before);
	TestEqual(TEXT("临时Working已归还"), Resources->Snapshot().Used[int32(EVoxelTerrainResourcePool::Working)], WorkingBefore);
	uint64 ExpectedRetained = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>() +
		VoxelBoundedAllocation::QuantizeBytes(Result.SurfaceMesh->Batches.GetAllocatedSize(), alignof(FVoxelRenderBatch));
	for (const auto& Batch : Result.SurfaceMesh->Batches)
	{
		const auto& Mesh = Batch.Mesh;
		ExpectedRetained += VoxelBoundedAllocation::QuantizeBytes(Mesh.Vertices.GetAllocatedSize(), alignof(FVector)) +
			VoxelBoundedAllocation::QuantizeBytes(Mesh.Triangles.GetAllocatedSize(), alignof(int32)) +
			VoxelBoundedAllocation::QuantizeBytes(Mesh.UV0.GetAllocatedSize(), alignof(FVector2D)) +
			Mesh.Normals.GetAllocatedSize() + Mesh.UV1.GetAllocatedSize() + Mesh.UV2.GetAllocatedSize() +
			Mesh.Colors.GetAllocatedSize() + Mesh.Tangents.GetAllocatedSize();
	}
	TestEqual(TEXT("原几何租约按编码实际容量结算"), Storage.GeometryLease->Bytes(),
		ExpectedRetained);
	return true;
}
#endif
