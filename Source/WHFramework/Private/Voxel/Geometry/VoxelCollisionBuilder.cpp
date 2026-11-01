#include "Voxel/Geometry/VoxelCollisionBuilder.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

bool FVoxelCollisionBuilder::Build(
	const FVoxelSectionSnapshot& InSnapshot,
	const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelShapeRegistry& InShapes,
	FVoxelSectionCollisionResult& OutResult,
	const TAtomic<bool>* InCancel)
{
	int32 BoxCount = 0;
	if (!VisitBoxes(InSnapshot, InRegistry, InShapes, [&BoxCount](const FBox&)
	{
		++BoxCount;
		return true;
	}, InCancel)) return false;

	// 候选成功前旧输出仍有效，容量授予必须覆盖两份数组同时存活的峰值。
	const uint64 CandidateBytes = VoxelBoundedAllocation::ArrayBytes<FBox>(BoxCount);
	const uint64 PeakBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionCollisionResult>() +
		OutResult.Boxes.GetAllocatedSize() + CandidateBytes;
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
				if (Definition && Definition->bSolid) { Candidate.UnknownHaloMask |= uint8(1u << Face); break; }
			}
		}
	}
	Candidate.Boxes.Reserve(BoxCount);
	check(uint64(Candidate.Boxes.GetAllocatedSize()) <= CandidateBytes);
	if (!VisitBoxes(InSnapshot, InRegistry, InShapes, [&Candidate, BoxCount](const FBox& Box)
	{
		if (Candidate.Boxes.Num() >= BoxCount) return false;
		Candidate.Boxes.Add(Box);
		return true;
	}, InCancel) || Candidate.Boxes.Num() != BoxCount || (InCancel && InCancel->Load())) return false;
	OutResult = MoveTemp(Candidate);
	if (OutResult.ResourceLease)
	{
		verify(OutResult.ResourceLease->Resize(OutResult.Bytes()));
	}
	return true;
}

bool FVoxelCollisionBuilder::VisitBoxes(
	const FVoxelSectionSnapshot& InSnapshot,
	const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelShapeRegistry& InShapes,
	TFunctionRef<bool(const FBox&)> InVisit,
	const TAtomic<bool>* InCancel)
{
	constexpr int32 MaxBoxes = 16384;
	if (InSnapshot.GridSide != VoxelBlock::Size || InSnapshot.Blocks.Num() != VoxelBlock::Volume ||
		(InCancel && InCancel->Load())) return false;
	int32 BoxCount = 0;
	const auto AddBox = [&InVisit, &BoxCount](const FBox& Box)
	{
		if (BoxCount == MaxBoxes || !InVisit(Box)) return false;
		++BoxCount;
		return true;
	};
	// 完整立方体才能证明相邻面被完全遮挡；特殊形状和未知邻区均保守视为可能通行。
	uint8 Occupancy[VoxelBlock::Volume] = {};
	uint8 Filled[VoxelBlock::Volume] = {};
	for (int32 Index = 0; Index < VoxelBlock::Volume; ++Index)
	{
		if (InCancel && InCancel->Load()) return false;
		const FVoxelBlockState State = FVoxelBlockState::Unpack(InSnapshot.Blocks[Index]);
		if (!InRegistry.IsValid(State)) return false;
		if (State.IsAir()) continue;
		const auto* Definition = InRegistry.Find(State.TypeId);
		if (!Definition->bSolid) continue;
		Occupancy[Index] = Definition->Shape == EVoxelShapeKind::FullCube ? 1 : 2;
	}
	bool bInvalidNeighbor = false;
	const auto IsFullyOccupied = [&](const FIntVector& Position)
	{
		if (Position.X >= 0 && Position.X < VoxelBlock::Size && Position.Y >= 0 && Position.Y < VoxelBlock::Size &&
			Position.Z >= 0 && Position.Z < VoxelBlock::Size)
		{
			return Occupancy[Position.X + VoxelBlock::Size * (Position.Y + VoxelBlock::Size * Position.Z)] == 1;
		}
		FVoxelBlockState Neighbor;
		if (!InSnapshot.TrySample(Position, Neighbor)) return false;
		if (!InRegistry.IsValid(Neighbor)) { bInvalidNeighbor = true; return false; }
		if (Neighbor.IsAir()) return false;
		const auto* Definition = InRegistry.Find(Neighbor.TypeId);
		return Definition && Definition->bSolid && Definition->Shape == EVoxelShapeKind::FullCube;
	};
	for (int32 Index = 0; Index < VoxelBlock::Volume; ++Index)
	{
		if (InCancel && InCancel->Load()) return false;
		if (Occupancy[Index] == 0) continue;
		const FIntVector Cell = VoxelCoord::Unlinear(uint16(Index));
		bool bExposed = false;
		for (int32 Axis = 0; Axis < 3 && !bExposed; ++Axis)
		{
			for (const int32 Direction : {-1, 1})
			{
				FIntVector Neighbor = Cell;
				Neighbor[Axis] += Direction;
				if (!IsFullyOccupied(Neighbor)) { bExposed = true; break; }
			}
		}
		if (bInvalidNeighbor) return false;
		if (!bExposed) continue;
		if (Occupancy[Index] == 1)
		{
			Filled[Index] = 1;
			continue;
		}
		const FVoxelBlockState State = FVoxelBlockState::Unpack(InSnapshot.Blocks[Index]);
		const auto* Shape = InRegistry.Find(State.TypeId)->FindShape(InShapes, State.State);
		if (!Shape) return false;
		const FVector Position(Cell);
		for (const FBox& Box : Shape->CollisionBoxes)
		{
			if (!AddBox(FBox(Box.Min + Position, Box.Max + Position))) return false;
		}
	}

	auto At = [&Filled](const int32 X, const int32 Y, const int32 Z) -> uint8&
	{
		return Filled[X + VoxelBlock::Size * Y + VoxelBlock::Size * VoxelBlock::Size * Z];
	};
	for (int32 Z = 0; Z < VoxelBlock::Size; ++Z)
	{
		for (int32 Y = 0; Y < VoxelBlock::Size; ++Y)
		{
			for (int32 X = 0; X < VoxelBlock::Size; ++X)
			{
				if (InCancel && InCancel->Load()) return false;
				if (!At(X, Y, Z)) continue;
				int32 Width = 1, Height = 1, Depth = 1;
				while (X + Width < VoxelBlock::Size && At(X + Width, Y, Z)) ++Width;
				bool bBlocked = false;
				while (Y + Height < VoxelBlock::Size && !bBlocked)
				{
					for (int32 DX = 0; DX < Width; ++DX)
					{
						if (!At(X + DX, Y + Height, Z)) { bBlocked = true; break; }
					}
					if (!bBlocked) ++Height;
				}
				bBlocked = false;
				while (Z + Depth < VoxelBlock::Size && !bBlocked)
				{
					for (int32 DY = 0; DY < Height && !bBlocked; ++DY)
					{
						for (int32 DX = 0; DX < Width; ++DX)
						{
							if (!At(X + DX, Y + DY, Z + Depth)) { bBlocked = true; break; }
						}
					}
					if (!bBlocked) ++Depth;
				}
				for (int32 DZ = 0; DZ < Depth; ++DZ)
				{
					for (int32 DY = 0; DY < Height; ++DY)
					{
						for (int32 DX = 0; DX < Width; ++DX) At(X + DX, Y + DY, Z + DZ) = 0;
					}
				}
				if (!AddBox(FBox(FVector(X, Y, Z), FVector(X + Width, Y + Height, Z + Depth)))) return false;
			}
		}
	}
	return true;
}
