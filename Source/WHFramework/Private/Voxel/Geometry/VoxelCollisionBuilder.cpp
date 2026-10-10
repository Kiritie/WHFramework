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
	if (InSnapshot.Blocks.Num() != VoxelBlock::Volume || (InCancel && InCancel->Load())) return false;
	int32 BoxCount = 0;
	const auto AddBox = [&InVisit, &BoxCount](const FBox& Box)
	{
		if (BoxCount == MaxBoxes || !InVisit(Box)) return false;
		++BoxCount;
		return true;
	};
	uint8 Filled[VoxelBlock::Volume] = {};
	for (int32 Index = 0; Index < VoxelBlock::Volume; ++Index)
	{
		if (InCancel && InCancel->Load()) return false;
		const FVoxelBlockState State = FVoxelBlockState::Unpack(InSnapshot.Blocks[Index]);
		if (!InRegistry.IsValid(State)) return false;
		if (State.IsAir()) continue;
		const auto* Definition = InRegistry.Find(State.TypeId);
		if (!Definition->bSolid) continue;
		if (Definition->Shape == EVoxelShapeKind::FullCube)
		{
			Filled[Index] = 1;
			continue;
		}
		const auto* Shape = InShapes.Find(Definition->Shape, State.State);
		if (!Shape) return false;
		const FVector Position(VoxelCoord::Unlinear(uint16(Index)));
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
