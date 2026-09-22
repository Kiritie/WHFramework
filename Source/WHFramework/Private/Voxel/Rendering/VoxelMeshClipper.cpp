#include "Voxel/Rendering/VoxelMeshClipper.h"

namespace
{
	struct FClipVertex
	{
		FVector Position;
		FVector Normal;
		FVector2D UV0;
		FVector2D UV1;
		FVector2D UV2;
		FLinearColor Color;
		FProcMeshTangent Tangent;
	};

	using FClipPolygon = TArray<FClipVertex, TInlineAllocator<9>>;

	FClipVertex Interpolate(const FClipVertex& InA, const FClipVertex& InB, const double InAlpha)
	{
		return {
			FMath::Lerp(InA.Position, InB.Position, InAlpha),
			FMath::Lerp(InA.Normal, InB.Normal, InAlpha).GetSafeNormal(),
			FMath::Lerp(InA.UV0, InB.UV0, InAlpha),
			FMath::Lerp(InA.UV1, InB.UV1, InAlpha),
			FMath::Lerp(InA.UV2, InB.UV2, InAlpha),
			FMath::Lerp(InA.Color, InB.Color, InAlpha),
			FProcMeshTangent(FMath::Lerp(InA.Tangent.TangentX, InB.Tangent.TangentX, InAlpha).GetSafeNormal(), InA.Tangent.bFlipTangentY)
		};
	}

	void Split(const FClipPolygon& InPolygon, const int32 InAxis, const double InPlane,
		const bool bInMinimum, FClipPolygon& OutInside, FClipPolygon& OutOutside)
	{
		OutInside.Reset();
		OutOutside.Reset();
		for (int32 Index = 0; Index < InPolygon.Num(); ++Index)
		{
			const FClipVertex& A = InPolygon[Index];
			const FClipVertex& B = InPolygon[(Index + 1) % InPolygon.Num()];
			const double DA = (A.Position[InAxis] - InPlane) * (bInMinimum ? 1.0 : -1.0);
			const double DB = (B.Position[InAxis] - InPlane) * (bInMinimum ? 1.0 : -1.0);
			const bool bAInside = DA >= 0.0;
			const bool bBInside = DB >= 0.0;
			(bAInside ? OutInside : OutOutside).Add(A);
			if (bAInside != bBInside)
			{
				const FClipVertex Crossing = Interpolate(A, B, DA / (DA - DB));
				OutInside.Add(Crossing);
				OutOutside.Add(Crossing);
			}
		}
	}

	void Append(const FClipPolygon& InPolygon, FVoxelMeshBuffers& OutMesh)
	{
		for (int32 Index = 1; Index + 1 < InPolygon.Num(); ++Index)
		{
			if (FVector::CrossProduct(InPolygon[Index].Position - InPolygon[0].Position,
				InPolygon[Index + 1].Position - InPolygon[0].Position).SizeSquared() < UE_DOUBLE_SMALL_NUMBER)
			{
				continue;
			}
			const int32 Base = OutMesh.Vertices.Num();
			for (const int32 VertexIndex : { 0, Index, Index + 1 })
			{
				const FClipVertex& Vertex = InPolygon[VertexIndex];
				OutMesh.Vertices.Add(Vertex.Position);
				OutMesh.Normals.Add(Vertex.Normal);
				OutMesh.UV0.Add(Vertex.UV0);
				OutMesh.UV1.Add(Vertex.UV1);
				OutMesh.UV2.Add(Vertex.UV2);
				OutMesh.Colors.Add(Vertex.Color);
				OutMesh.Tangents.Add(Vertex.Tangent);
			}
			OutMesh.Triangles.Append({ Base, Base + 1, Base + 2 });
		}
	}
}

void VoxelMeshClipper::NormalizeBoxes(TArray<FBox>& InOutBoxes)
{
	// 合并共面的相邻覆盖区，避免每个粗三角形重复检查数百个细分块。
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const int32 U = (Axis + 1) % 3;
		const int32 V = (Axis + 2) % 3;
		InOutBoxes.Sort([Axis, U, V](const FBox& A, const FBox& B)
		{
			for (const int32 Component : { U, V })
			{
				if (A.Min[Component] != B.Min[Component]) return A.Min[Component] < B.Min[Component];
				if (A.Max[Component] != B.Max[Component]) return A.Max[Component] < B.Max[Component];
			}
			return A.Min[Axis] < B.Min[Axis];
		});
		int32 Count = 0;
		for (const FBox& Box : InOutBoxes)
		{
			if (Count > 0)
			{
				FBox& Previous = InOutBoxes[Count - 1];
				if (Previous.Min[U] == Box.Min[U] && Previous.Max[U] == Box.Max[U] &&
					Previous.Min[V] == Box.Min[V] && Previous.Max[V] == Box.Max[V] && Previous.Max[Axis] >= Box.Min[Axis])
				{
					Previous.Max[Axis] = FMath::Max(Previous.Max[Axis], Box.Max[Axis]);
					continue;
				}
			}
			InOutBoxes[Count++] = Box;
		}
		InOutBoxes.SetNum(Count, EAllowShrinking::No);
	}
}

void VoxelMeshClipper::Subtract(const FVoxelSectionMeshResult& InMesh,
	TConstArrayView<FBox> InBoxes, FVoxelSectionMeshResult& OutMesh, const TAtomic<bool>* InCancel)
{
	OutMesh = FVoxelSectionMeshResult();
	OutMesh.Stamp = InMesh.Stamp;
	for (const FVoxelRenderBatch& Batch : InMesh.Batches)
	{
		FVoxelRenderBatch& Output = OutMesh.Batches.AddDefaulted_GetRef();
		Output.Group = Batch.Group;
		Output.Bank = Batch.Bank;
		const FVoxelMeshBuffers& Mesh = Batch.Mesh;
		for (int32 Triangle = 0; Triangle < Mesh.Triangles.Num(); Triangle += 3)
		{
			if (InCancel && InCancel->Load())
			{
				return;
			}
			FClipPolygon Polygon;
			FBox Bounds(ForceInit);
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 Index = Mesh.Triangles[Triangle + Corner];
				Polygon.Add({ Mesh.Vertices[Index], Mesh.Normals[Index], Mesh.UV0[Index],
					Mesh.UV1[Index], Mesh.UV2[Index], Mesh.Colors[Index], Mesh.Tangents[Index] });
				Bounds += Mesh.Vertices[Index];
			}
			TArray<FClipPolygon, TInlineAllocator<8>> Fragments;
			Fragments.Add(MoveTemp(Polygon));
			for (const FBox& Box : InBoxes)
			{
				if (!Bounds.Intersect(Box)) continue;
				TArray<FClipPolygon, TInlineAllocator<8>> Remaining;
				for (const FClipPolygon& Fragment : Fragments)
				{
					FClipPolygon Inside = Fragment;
					for (int32 Plane = 0; Plane < 6 && Inside.Num() >= 3; ++Plane)
					{
						FClipPolygon NextInside;
						FClipPolygon Outside;
						const int32 Axis = Plane / 2;
						const bool bMinimum = Plane % 2 == 0;
						Split(Inside, Axis, bMinimum ? Box.Min[Axis] : Box.Max[Axis], bMinimum, NextInside, Outside);
						if (Outside.Num() >= 3) Remaining.Add(MoveTemp(Outside));
						Inside = MoveTemp(NextInside);
					}
				}
				Fragments = MoveTemp(Remaining);
				if (Fragments.IsEmpty()) break;
			}
			for (const FClipPolygon& Fragment : Fragments) Append(Fragment, Output.Mesh);
		}
	}
}
