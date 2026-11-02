#include "Voxel/Geometry/VoxelCollisionBuilder.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Geometry/VoxelFaceMerger.h"
#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

bool FVoxelSectionCollisionResult::BindFineGeometry(
	TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> InGeometry, const FIntVector& InSection)
{
	if (!InGeometry || !InGeometry->bCoverageValidated || !Vertices.IsEmpty() || !Triangles.IsEmpty()) return false;
	const FIntVector SectionMin = InSection * VoxelBlock::Size;
	int64 Volume = 0;
	int64 VertexCount = 0;
	int64 IndexCount = 0;
	FBox Bounds(ForceInit);
	for (const FVoxelFineMeshPiece& Piece : InGeometry->Pieces)
	{
		const FIntVector Owner(int32(FVoxelFineSpatial::FloorDivide(Piece.Key.Min.X, VoxelBlock::Size)),
			int32(FVoxelFineSpatial::FloorDivide(Piece.Key.Min.Y, VoxelBlock::Size)),
			int32(FVoxelFineSpatial::FloorDivide(Piece.Key.Min.Z, VoxelBlock::Size)));
		if (Owner != InSection) continue;
		const FIntVector Offset = Piece.Key.Min - SectionMin;
		if (!Piece.bCollisionMeshCompatible || !Piece.Mesh || !Piece.Mesh->bValidated || Piece.Key.SideCells <= 0 ||
			Offset.GetMin() < 0 || Offset.GetMax() + Piece.Key.SideCells > VoxelBlock::Size) return false;
		Volume += int64(Piece.Key.SideCells) * Piece.Key.SideCells * Piece.Key.SideCells;
		bool bHasBlockingFaces = false;
		for (const FVoxelRenderBatch& Batch : Piece.Mesh->Batches)
		{
			if (Batch.CollisionMode != EVoxelMeshCollisionMode::Blocking) continue;
			if (Batch.Mesh.Triangles.Num() % 3 != 0) return false;
			VertexCount += Batch.Mesh.Vertices.Num();
			IndexCount += Batch.Mesh.Triangles.Num();
			bHasBlockingFaces |= !Batch.Mesh.Triangles.IsEmpty();
		}
		if (bHasBlockingFaces)
		{
			if (!Piece.Mesh->LocalBounds.IsValid) return false;
			Bounds += FBox(Piece.Mesh->LocalBounds.Min + FVector(Offset), Piece.Mesh->LocalBounds.Max + FVector(Offset));
		}
	}
	if (Volume != VoxelBlock::Volume || VertexCount > MAX_int32 || IndexCount > MAX_int32 ||
		(IndexCount && (!VertexCount || !Bounds.IsValid))) return false;
	FineVertexCount = int32(VertexCount);
	FineIndexCount = int32(IndexCount);
	LocalBounds = Bounds;
	FineGeometry = MoveTemp(InGeometry);
	return true;
}

uint64 FVoxelCollisionBuilder::MaximumOutputBytes()
{
	return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionCollisionResult>() +
		VoxelBoundedAllocation::ArrayBytes<FVector3f>(MaximumQuads * 4) +
		VoxelBoundedAllocation::ArrayBytes<int32>(MaximumQuads * 6);
}

bool FVoxelCollisionBuilder::Build(
	const FVoxelSectionSnapshot& InSnapshot,
	const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelShapeRegistry& InShapes,
	FVoxelSectionCollisionResult& OutResult,
	const TAtomic<bool>* InCancel)
{
	int32 QuadCount = 0;
	if (!VisitQuads(InSnapshot, InRegistry, InShapes, [&QuadCount](const FVector*)
	{
		++QuadCount;
		return true;
	}, InCancel)) return false;

	const uint64 VertexBytes = VoxelBoundedAllocation::ArrayBytes<FVector3f>(QuadCount * 4);
	const uint64 IndexBytes = VoxelBoundedAllocation::ArrayBytes<int32>(QuadCount * 6);
	const uint64 PeakBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionCollisionResult>() +
		OutResult.Vertices.GetAllocatedSize() + OutResult.Triangles.GetAllocatedSize() + VertexBytes + IndexBytes;
	if (OutResult.ResourceLease && OutResult.ResourceLease->Bytes() < PeakBytes) return false;

	FVoxelSectionCollisionResult Candidate;
	Candidate.ResourceLease = OutResult.ResourceLease;
	Candidate.Stamp = OutResult.Stamp;
	Candidate.Stamp.WorldEpoch = InSnapshot.Stamp.Epoch;
	Candidate.Stamp.Token = InSnapshot.Stamp.Token;
	Candidate.Stamp.Revision = InSnapshot.Revision;
	Candidate.Stamp.Section = InSnapshot.Section;
	Candidate.Stamp.RegistryRevision = InRegistry.Hash;
	Candidate.Stamp.ShapeRevision = InShapes.GetContentHash();
	for (int32 Face = 0; Face < 6; ++Face)
	{
		if (InSnapshot.Known[Face]) continue;
		const int32 Axis = Face / 2;
		const int32 UAxis = (Axis + 1) % 3;
		const int32 VAxis = (Axis + 2) % 3;
		for (int32 V = 0; V < VoxelBlock::Size && !(Candidate.UnknownHaloMask & (1u << Face)); ++V)
		{
			for (int32 U = 0; U < VoxelBlock::Size; ++U)
			{
				FIntVector Cell = FIntVector::ZeroValue;
				Cell[Axis] = (Face & 1) ? 0 : VoxelBlock::Size - 1;
				Cell[UAxis] = U;
				Cell[VAxis] = V;
				const int32 Index = Cell.X + VoxelBlock::Size * (Cell.Y + VoxelBlock::Size * Cell.Z);
				const FVoxelBlockState State = FVoxelBlockState::Unpack(InSnapshot.Blocks[Index]);
				const auto* Definition = State.IsAir() ? nullptr : InRegistry.Find(State.TypeId);
				if (!Definition || !Definition->bSolid) continue;
				const auto* Shape = Definition->FindShape(InShapes, State.State);
				if (Shape && !Shape->CollisionBoxes.IsEmpty())
				{
					Candidate.UnknownHaloMask |= uint8(1u << Face);
					break;
				}
			}
		}
	}
	Candidate.Vertices.Reserve(QuadCount * 4);
	Candidate.Triangles.Reserve(QuadCount * 6);
	check(uint64(Candidate.Vertices.GetAllocatedSize()) <= VertexBytes);
	check(uint64(Candidate.Triangles.GetAllocatedSize()) <= IndexBytes);
	if (!VisitQuads(InSnapshot, InRegistry, InShapes, [&Candidate, QuadCount](const FVector* Points)
	{
		if (Candidate.Vertices.Num() / 4 >= QuadCount) return false;
		const int32 Base = Candidate.Vertices.Num();
		for (int32 Index = 0; Index < 4; ++Index)
		{
			if (Points[Index].ContainsNaN()) return false;
			Candidate.Vertices.Add(FVector3f(Points[Index]));
			Candidate.LocalBounds += Points[Index];
		}
		// 顶点顺序与 Fine 可视表面相同；Chaos 物理三角形使用外向绕序。
		Candidate.Triangles.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
		return true;
	}, InCancel) || Candidate.Vertices.Num() != QuadCount * 4 ||
		Candidate.Triangles.Num() != QuadCount * 6 || (InCancel && InCancel->Load())) return false;
	OutResult = MoveTemp(Candidate);
	if (OutResult.ResourceLease) verify(OutResult.ResourceLease->Resize(OutResult.Bytes()));
	return true;
}

bool FVoxelCollisionBuilder::VisitQuads(
	const FVoxelSectionSnapshot& InSnapshot,
	const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelShapeRegistry& InShapes,
	TFunctionRef<bool(const FVector*)> InVisit,
	const TAtomic<bool>* InCancel)
{
	if (InSnapshot.GridSide != VoxelBlock::Size || InSnapshot.Blocks.Num() != VoxelBlock::Volume ||
		(InCancel && InCancel->Load())) return false;
	int32 QuadCount = 0;
	const auto Emit = [&InVisit, &QuadCount](const FVector* Points)
	{
		if (QuadCount == MaximumQuads || !InVisit(Points)) return false;
		++QuadCount;
		return true;
	};
	bool bInvalidNeighbor = false;
	const auto Classify = [&](const FVoxelBlockState State) -> uint8
	{
		if (!InRegistry.IsValid(State)) { bInvalidNeighbor = true; return 0; }
		if (State.IsAir()) return 0;
		const auto* Definition = InRegistry.Find(State.TypeId);
		if (!Definition->bSolid) return 0;
		if (Definition->Shape == EVoxelShapeKind::FullCube && Definition->StateGeometry.IsEmpty()) return 1;
		const auto* Shape = Definition->FindShape(InShapes, State.State);
		if (!Shape) { bInvalidNeighbor = true; return 0; }
		if (Shape->CollisionBoxes.IsEmpty()) return 0;
		if (Shape->CollisionBoxes.Num() == 1 && Shape->CollisionBoxes[0].Min == FVector::ZeroVector &&
			Shape->CollisionBoxes[0].Max == FVector::OneVector) return 1;
		return 2;
	};
	uint8 Occupancy[VoxelBlock::Volume] = {};
	for (int32 Index = 0; Index < VoxelBlock::Volume; ++Index)
	{
		if (InCancel && InCancel->Load()) return false;
		const FVoxelBlockState State = FVoxelBlockState::Unpack(InSnapshot.Blocks[Index]);
		Occupancy[Index] = Classify(State);
		if (bInvalidNeighbor) return false;
	}
	const auto NeighborOccupancy = [&](const FIntVector& Position) -> uint8
	{
		if (Position.X >= 0 && Position.X < VoxelBlock::Size && Position.Y >= 0 && Position.Y < VoxelBlock::Size &&
			Position.Z >= 0 && Position.Z < VoxelBlock::Size)
			return Occupancy[Position.X + VoxelBlock::Size * (Position.Y + VoxelBlock::Size * Position.Z)];
		FVoxelBlockState Neighbor;
		return InSnapshot.TrySample(Position, Neighbor) ? Classify(Neighbor) : 0;
	};
	const auto NeighborShape = [&](const FIntVector& Position) -> const FVoxelResolvedShape*
	{
		FVoxelBlockState Neighbor;
		if (!InSnapshot.TrySample(Position, Neighbor) || Neighbor.IsAir()) return nullptr;
		const auto* Definition = InRegistry.Find(Neighbor.TypeId);
		const auto* Shape = Definition ? Definition->FindShape(InShapes, Neighbor.State) : nullptr;
		if (!Shape) bInvalidNeighbor = true;
		return Shape;
	};
	const auto EmitRectangle = [&Emit](const uint8 Face, const double Plane, const double UMin, const double UMax,
		const double VMin, const double VMax)
	{
		const int32 Axis = Face / 2, UAxis = (Axis + 1) % 3, VAxis = (Axis + 2) % 3;
		const double Us[4] = {UMin, UMax, UMax, UMin};
		const double Vs[4] = {VMin, VMin, VMax, VMax};
		FVector Points[4];
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Points[Index] = FVector::ZeroVector;
			Points[Index][Axis] = Plane;
			Points[Index][UAxis] = Us[Index];
			Points[Index][VAxis] = Vs[Index];
		}
		if (Face & 1) Swap(Points[1], Points[3]);
		return Emit(Points);
	};
	// 在真实碰撞盒边界上切分矩形，避免半砖等相邻形状留下内部碰撞面。
	const auto EmitUncovered = [&](const uint8 Face, const double Plane, const double UMin, const double UMax,
		const double VMin, const double VMax, const TArray<FBox>* SelfBoxes, const FIntVector& SelfCell,
		const int32 SelfIndex, const TArray<FBox>* NeighborBoxes, const FIntVector& NeighborCell)
	{
		const int32 Axis = Face / 2, UAxis = (Axis + 1) % 3, VAxis = (Axis + 2) % 3;
		const int32 SelfCount = SelfBoxes ? SelfBoxes->Num() : 0;
		const int32 Total = SelfCount + (NeighborBoxes ? NeighborBoxes->Num() : 0);
		const auto Partition = [&](auto&& Recurse, const double U0, const double U1,
			const double V0, const double V1, const int32 Start) -> bool
		{
			if (InCancel && InCancel->Load()) return false;
			for (int32 Index = Start; Index < Total; ++Index)
			{
				const bool bSelf = Index < SelfCount;
				const int32 BoxIndex = bSelf ? Index : Index - SelfCount;
				if (bSelf && BoxIndex == SelfIndex) continue;
				const FBox& Box = bSelf ? (*SelfBoxes)[BoxIndex] : (*NeighborBoxes)[BoxIndex];
				const FIntVector& Offset = bSelf ? SelfCell : NeighborCell;
				const double MinAxis = Box.Min[Axis] + Offset[Axis];
				const double MaxAxis = Box.Max[Axis] + Offset[Axis];
				const bool bAcross = (Face & 1) ? MaxAxis >= Plane && MinAxis < Plane : MinAxis <= Plane && MaxAxis > Plane;
				const bool bEarlierCoplanar = bSelf && BoxIndex < SelfIndex &&
					((Face & 1) ? MinAxis == Plane : MaxAxis == Plane);
				if (!bAcross && !bEarlierCoplanar) continue;
				const double CutU0 = FMath::Max(U0, Box.Min[UAxis] + Offset[UAxis]);
				const double CutU1 = FMath::Min(U1, Box.Max[UAxis] + Offset[UAxis]);
				const double CutV0 = FMath::Max(V0, Box.Min[VAxis] + Offset[VAxis]);
				const double CutV1 = FMath::Min(V1, Box.Max[VAxis] + Offset[VAxis]);
				if (CutU0 >= CutU1 || CutV0 >= CutV1) continue;
				return (U0 >= CutU0 || Recurse(Recurse, U0, CutU0, V0, V1, Index + 1)) &&
					(CutU1 >= U1 || Recurse(Recurse, CutU1, U1, V0, V1, Index + 1)) &&
					(V0 >= CutV0 || Recurse(Recurse, CutU0, CutU1, V0, CutV0, Index + 1)) &&
					(CutV1 >= V1 || Recurse(Recurse, CutU0, CutU1, CutV1, V1, Index + 1));
			}
			return EmitRectangle(Face, Plane, U0, U1, V0, V1);
		};
		return Partition(Partition, UMin, UMax, VMin, VMax, 0);
	};
	uint8 Mask[VoxelBlock::Size * VoxelBlock::Size] = {};
	for (uint8 Face = 0; Face < 6; ++Face)
	{
		const int32 Axis = Face / 2, UAxis = (Axis + 1) % 3, VAxis = (Axis + 2) % 3;
		for (int32 Slice = 0; Slice < VoxelBlock::Size; ++Slice)
		{
			if (InCancel && InCancel->Load()) return false;
			for (int32 V = 0; V < VoxelBlock::Size; ++V)
				for (int32 U = 0; U < VoxelBlock::Size; ++U)
				{
					FIntVector Cell = FIntVector::ZeroValue;
					Cell[Axis] = Slice; Cell[UAxis] = U; Cell[VAxis] = V;
					const int32 Index = Cell.X + VoxelBlock::Size * (Cell.Y + VoxelBlock::Size * Cell.Z);
					Mask[U + VoxelBlock::Size * V] = 0;
					if (Occupancy[Index] != 1) continue;
					const FIntVector NeighborCell = Cell + VoxelCoord::Direction(Face);
					const uint8 Neighbor = NeighborOccupancy(NeighborCell);
					if (Neighbor == 0) Mask[U + VoxelBlock::Size * V] = 1;
					else if (Neighbor == 2)
					{
						const auto* Shape = NeighborShape(NeighborCell);
						if (!Shape || !EmitUncovered(Face, Slice + ((Face & 1) ? 0 : 1), U, U + 1, V, V + 1,
							nullptr, Cell, INDEX_NONE, &Shape->CollisionBoxes, NeighborCell)) return false;
					}
				}
			if (bInvalidNeighbor) return false;
			for (int32 V = 0; V < VoxelBlock::Size; ++V)
				for (int32 U = 0; U < VoxelBlock::Size;)
				{
					if (!Mask[U + VoxelBlock::Size * V]) { ++U; continue; }
					FIntRect Rect;
					if (!FVoxelFaceMerger::Grow(FIntPoint(U, V), FIntPoint(VoxelBlock::Size),
						[&Mask](int32 X, int32 Y) { return Mask[X + VoxelBlock::Size * Y] != 0; }, Rect, InCancel)) return false;
					if (!EmitRectangle(Face, Slice + ((Face & 1) ? 0 : 1), Rect.Min.X, Rect.Max.X, Rect.Min.Y, Rect.Max.Y)) return false;
					for (int32 Y = Rect.Min.Y; Y < Rect.Max.Y; ++Y)
						for (int32 X = Rect.Min.X; X < Rect.Max.X; ++X) Mask[X + VoxelBlock::Size * Y] = 0;
					U = Rect.Max.X;
				}
		}
	}
	for (int32 Index = 0; Index < VoxelBlock::Volume; ++Index)
	{
		if (InCancel && InCancel->Load()) return false;
		if (Occupancy[Index] != 2) continue;
		const FIntVector Cell = VoxelCoord::Unlinear(uint16(Index));
		const FVoxelBlockState State = FVoxelBlockState::Unpack(InSnapshot.Blocks[Index]);
		const auto* Definition = InRegistry.Find(State.TypeId);
		const auto* Shape = Definition ? Definition->FindShape(InShapes, State.State) : nullptr;
		if (!Shape) return false;
		if (Shape->CollisionBoxes.IsEmpty()) continue;
		for (const FBox& Box : Shape->CollisionBoxes)
		{
			if (!Box.IsValid || Box.Min.ContainsNaN() || Box.Max.ContainsNaN() ||
				Box.Min.X >= Box.Max.X || Box.Min.Y >= Box.Max.Y || Box.Min.Z >= Box.Max.Z) return false;
		}
		if (!Shape->Quads.IsEmpty() && Shape->CollisionBoxes == Shape->SelectionBoxes)
		{
			for (const FVoxelShapeQuad& Quad : Shape->Quads)
			{
				const FIntVector NeighborCell = Cell + VoxelCoord::Direction(Quad.Face);
				const uint8 Neighbor = Quad.bBoundary ? NeighborOccupancy(NeighborCell) : 0;
				if (bInvalidNeighbor) return false;
				if (Neighbor == 1) continue;
				if (Neighbor == 2)
				{
					const auto* Other = NeighborShape(NeighborCell);
					const int32 Axis = Quad.Face / 2, UAxis = (Axis + 1) % 3, VAxis = (Axis + 2) % 3;
					FBox Bounds(ForceInit);
					for (const FVector& Vertex : Quad.Vertices) Bounds += Vertex + FVector(Cell);
					if (!Other || !EmitUncovered(Quad.Face, Bounds.Min[Axis], Bounds.Min[UAxis], Bounds.Max[UAxis],
						Bounds.Min[VAxis], Bounds.Max[VAxis], nullptr, Cell, INDEX_NONE,
						&Other->CollisionBoxes, NeighborCell)) return false;
				}
				else
				{
					FVector Points[4];
					for (int32 Vertex = 0; Vertex < 4; ++Vertex) Points[Vertex] = Quad.Vertices[Vertex] + FVector(Cell);
					if (!Emit(Points)) return false;
				}
			}
			continue;
		}
		for (int32 BoxIndex = 0; BoxIndex < Shape->CollisionBoxes.Num(); ++BoxIndex)
		{
			const FBox& Box = Shape->CollisionBoxes[BoxIndex];
			for (uint8 Face = 0; Face < 6; ++Face)
			{
				const int32 Axis = Face / 2, UAxis = (Axis + 1) % 3, VAxis = (Axis + 2) % 3;
				const double Plane = (Face & 1) ? Box.Min[Axis] : Box.Max[Axis];
				const bool bBoundary = Plane == ((Face & 1) ? 0.0 : 1.0);
				const FIntVector NeighborCell = Cell + VoxelCoord::Direction(Face);
				const uint8 Neighbor = bBoundary ? NeighborOccupancy(NeighborCell) : 0;
				if (bInvalidNeighbor) return false;
				if (Neighbor == 1) continue;
				const auto* Other = Neighbor == 2 ? NeighborShape(NeighborCell) : nullptr;
				if ((Neighbor == 2 && !Other) || !EmitUncovered(Face, Cell[Axis] + Plane,
					Cell[UAxis] + Box.Min[UAxis], Cell[UAxis] + Box.Max[UAxis],
					Cell[VAxis] + Box.Min[VAxis], Cell[VAxis] + Box.Max[VAxis],
					&Shape->CollisionBoxes, Cell, BoxIndex, Other ? &Other->CollisionBoxes : nullptr, NeighborCell)) return false;
			}
		}
	}
	return true;
}
