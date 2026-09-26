#include "Voxel/Geometry/DWVoxelBoundaryTransition.h"

#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Runtime/VoxelSection.h"

namespace
{
	constexpr int32 SectionSide = 16;

	uint8 Opposite(const uint8 InFace)
	{
		return InFace ^ 1;
	}

	bool CaptureStates(const FVoxelViewKey& InKey, const uint8 InFace,
		const TArray<FVoxelBlockState>& InBlocks, const uint64 InRevision,
		FVoxelBoundaryFaceSnapshot& OutSnapshot)
	{
		if (InFace >= 6 || InBlocks.Num() != SectionSide * SectionSide * SectionSide)
		{
			return false;
		}
		FVoxelBoundaryFaceSnapshot Snapshot;
		Snapshot.Key = InKey;
		Snapshot.Face = InFace;
		Snapshot.Revision = InRevision;
		Snapshot.States.SetNumUninitialized(SectionSide * SectionSide);
		const int32 Axis = InFace / 2;
		const int32 UAxis = (Axis + 1) % 3;
		const int32 VAxis = (Axis + 2) % 3;
		for (int32 V = 0; V < SectionSide; ++V)
		{
			for (int32 U = 0; U < SectionSide; ++U)
			{
				FIntVector Local = FIntVector::ZeroValue;
				Local[Axis] = (InFace & 1) ? 0 : SectionSide - 1;
				Local[UAxis] = U;
				Local[VAxis] = V;
				Snapshot.States[U + SectionSide * V] =
					InBlocks[VoxelCoord::Linear(Local)].Pack();
			}
		}
		OutSnapshot = MoveTemp(Snapshot);
		return true;
	}
}

bool FVoxelBoundaryFaceSnapshot::CaptureFine(const FVoxelViewKey& InKey,
	const uint8 InFace, const FVoxelSection& InSection,
	FVoxelBoundaryFaceSnapshot& OutSnapshot)
{
	if (InKey.Level != 0 || InSection.Status != EVoxelSectionStatus::DataReady)
	{
		return false;
	}
	return CaptureStates(InKey, InFace, InSection.Blocks,
		InSection.CommittedRevision, OutSnapshot);
}

bool FVoxelBoundaryFaceSnapshot::CaptureProxy(const FVoxelVoxelProxyData& InData,
	const uint8 InFace, FVoxelBoundaryFaceSnapshot& OutSnapshot)
{
	return CaptureStates(InData.Key, InFace, InData.Cells,
		InData.Revision, OutSnapshot);
}

bool FVoxelBoundaryFaceSnapshot::Sample(const FIntVector& InWorldCell,
	FVoxelBlockState& OutState) const
{
	if (Face >= 6 || States.Num() != SectionSide * SectionSide) return false;
	const FVoxelGenerationBounds Bounds = Key.GetBounds();
	const int32 Axis = Face / 2;
	const int32 UAxis = (Axis + 1) % 3;
	const int32 VAxis = (Axis + 2) % 3;
	const int32 BoundaryCell = (Face & 1) ? Bounds.Min[Axis] : Bounds.Max[Axis] - 1;
	if (InWorldCell[Axis] != BoundaryCell ||
		InWorldCell[UAxis] < Bounds.Min[UAxis] ||
		InWorldCell[UAxis] >= Bounds.Max[UAxis] ||
		InWorldCell[VAxis] < Bounds.Min[VAxis] ||
		InWorldCell[VAxis] >= Bounds.Max[VAxis]) return false;
	const int32 U = (InWorldCell[UAxis] - Bounds.Min[UAxis]) / Key.GetStep();
	const int32 V = (InWorldCell[VAxis] - Bounds.Min[VAxis]) / Key.GetStep();
	OutState = FVoxelBlockState::Unpack(States[U + SectionSide * V]);
	return true;
}

bool FVoxelBoundaryTransitionContext::CoversCell(const uint8 InFace,
	const FIntVector& InLocalCell) const
{
	if (InFace >= 6) return false;
	const int32 Axis = InFace / 2;
	if (InLocalCell[Axis] != ((InFace & 1) ? 0 : SectionSide - 1)) return false;
	const FVoxelGenerationBounds Bounds = Owner.GetBounds();
	const FIntVector CellMin = Bounds.Min + InLocalCell * Owner.GetStep();
	const int32 UAxis = (Axis + 1) % 3;
	const int32 VAxis = (Axis + 2) % 3;
	for (const FVoxelBoundaryTransitionPatch& Patch : Patches)
	{
		if (Patch.Face.Owner != Owner ||
			static_cast<uint8>(Patch.Face.Direction) != InFace) continue;
		if (CellMin[UAxis] >= Patch.Face.Min[UAxis] &&
			CellMin[UAxis] + Owner.GetStep() <= Patch.Face.Max[UAxis] &&
			CellMin[VAxis] >= Patch.Face.Min[VAxis] &&
			CellMin[VAxis] + Owner.GetStep() <= Patch.Face.Max[VAxis]) return true;
	}
	return false;
}

bool FVoxelBoundaryTransitionContext::Validate() const
{
	const FVoxelGenerationBounds OwnerBounds = Owner.GetBounds();
	for (const FVoxelBoundaryTransitionPatch& Patch : Patches)
	{
		const uint8 Face = static_cast<uint8>(Patch.Face.Direction);
		if (Face >= 6) return false;
		const int32 Axis = Face / 2;
		const int32 UAxis = (Axis + 1) % 3;
		const int32 VAxis = (Axis + 2) % 3;
		const FVoxelGenerationBounds NeighborBounds = Patch.Neighbor.Key.GetBounds();
		if (Patch.Face.Owner != Owner ||
			Patch.Face.Neighbor != Patch.Neighbor.Key ||
			Owner.Level <= Patch.Neighbor.Key.Level ||
			Patch.Neighbor.Face != Opposite(Face) ||
			Patch.Neighbor.States.Num() != SectionSide * SectionSide ||
			Patch.Face.Ratio < 2 || Patch.Face.Ratio > SectionSide ||
			!FMath::IsPowerOfTwo(Patch.Face.Ratio) ||
			Owner.GetStep() / Patch.Face.Ratio != Patch.Neighbor.Key.GetStep() ||
			((Face & 1) ? OwnerBounds.Min[Axis] != NeighborBounds.Max[Axis]
				: OwnerBounds.Max[Axis] != NeighborBounds.Min[Axis]) ||
			Patch.Face.Min != NeighborBounds.Min ||
			Patch.Face.Max != NeighborBounds.Max ||
			Patch.Face.Min[UAxis] < OwnerBounds.Min[UAxis] ||
			Patch.Face.Max[UAxis] > OwnerBounds.Max[UAxis] ||
			Patch.Face.Min[VAxis] < OwnerBounds.Min[VAxis] ||
			Patch.Face.Max[VAxis] > OwnerBounds.Max[VAxis] ||
			(Patch.Face.Min[UAxis] - OwnerBounds.Min[UAxis]) % Owner.GetStep() != 0 ||
			(Patch.Face.Max[UAxis] - OwnerBounds.Min[UAxis]) % Owner.GetStep() != 0 ||
			(Patch.Face.Min[VAxis] - OwnerBounds.Min[VAxis]) % Owner.GetStep() != 0 ||
			(Patch.Face.Max[VAxis] - OwnerBounds.Min[VAxis]) % Owner.GetStep() != 0)
		{
			return false;
		}
	}
	for (int32 A = 0; A < Patches.Num(); ++A)
	{
		for (int32 B = A + 1; B < Patches.Num(); ++B)
		{
			const FVoxelVolumeTransitionFace& First = Patches[A].Face;
			const FVoxelVolumeTransitionFace& Second = Patches[B].Face;
			if (First.Direction != Second.Direction) continue;
			const int32 Axis = static_cast<uint8>(First.Direction) / 2;
			const int32 UAxis = (Axis + 1) % 3;
			const int32 VAxis = (Axis + 2) % 3;
			if (FMath::Max(First.Min[UAxis], Second.Min[UAxis]) <
				FMath::Min(First.Max[UAxis], Second.Max[UAxis]) &&
				FMath::Max(First.Min[VAxis], Second.Min[VAxis]) <
				FMath::Min(First.Max[VAxis], Second.Max[VAxis])) return false;
		}
	}
	return true;
}

uint64 FVoxelBoundaryTransitionContext::Signature(const uint64 InOwnerRevision) const
{
	uint64 Hash = 1469598103934665603ull;
	auto Mix = [&Hash](const uint64 Value)
	{
		Hash ^= Value;
		Hash *= 1099511628211ull;
	};
	Mix(GetTypeHash(Owner));
	Mix(InOwnerRevision);
	for (const FVoxelBoundaryTransitionPatch& Patch : Patches)
	{
		Mix(GetTypeHash(Patch.Face.Neighbor));
		Mix(static_cast<uint8>(Patch.Face.Direction));
		Mix(Patch.Face.Ratio);
		Mix(Patch.Neighbor.Revision);
	}
	return Hash;
}
