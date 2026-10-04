#include "Voxel/Geometry/DWVoxelBoundaryTransition.h"

#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Runtime/VoxelSection.h"

namespace
{


	uint8 Opposite(const uint8 InFace)
	{
		return InFace ^ 1;
	}

	bool CaptureStates(const FVoxelViewKey& InKey, const uint8 InFace,
		const TArray<FVoxelBlockState>& InBlocks, const uint64 InRevision, const int32 InStep,
		FVoxelBoundaryFaceSnapshot& OutSnapshot)
	{
		const int32 SectionSide = InKey.GetSide() / InStep;
		if (InFace >= 6 || InBlocks.Num() != SectionSide * SectionSide * SectionSide)
		{
			return false;
		}
		FVoxelBoundaryFaceSnapshot Snapshot;
		Snapshot.Key = InKey;
		Snapshot.Face = InFace;
		Snapshot.Revision = InRevision;
		Snapshot.SampleStepCells = InStep;
		Snapshot.GridSide = SectionSide;
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
					InBlocks[Local.X + SectionSide * (Local.Y + SectionSide * Local.Z)].Pack();
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
		InSection.CommittedRevision, 1, OutSnapshot);
}

bool FVoxelBoundaryFaceSnapshot::CaptureProxy(const FVoxelVoxelProxyData& InData,
	const uint8 InFace, FVoxelBoundaryFaceSnapshot& OutSnapshot)
{
	return CaptureStates(InData.Key, InFace, InData.Cells,
		InData.Revision, InData.Geometry.SampleStepCells, OutSnapshot);
}

bool FVoxelBoundaryFaceSnapshot::Sample(const FIntVector& InWorldCell,
	FVoxelBlockState& OutState) const
{
	const int32 Step = SampleStepCells > 0 ? SampleStepCells : Key.GetSampleStep();
	const int32 SectionSide = GridSide > 0 ? GridSide : Key.GetSide() / Step;
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
	const int32 U = (InWorldCell[UAxis] - Bounds.Min[UAxis]) / Step;
	const int32 V = (InWorldCell[VAxis] - Bounds.Min[VAxis]) / Step;
	OutState = FVoxelBlockState::Unpack(States[U + SectionSide * V]);
	return true;
}

bool FVoxelBoundaryTransitionContext::CoversCell(const uint8 InFace,
	const FIntVector& InLocalCell) const
{
	const int32 SectionSide = GetGridSide();
	if (InFace >= 6) return false;
	const int32 Axis = InFace / 2;

	const FVoxelGenerationBounds Bounds = Owner.GetBounds();
	const FIntVector CellMin = Bounds.Min + InLocalCell * GetSampleStep();
	const int32 UAxis = (Axis + 1) % 3;
	const int32 VAxis = (Axis + 2) % 3;
	for (const auto& Face : SuppressedFaces)
	{
		if (static_cast<uint8>(Face.Direction) != InFace || CellMin[Axis] + ((InFace & 1) ? 0 : GetSampleStep()) != Face.Min[Axis]) continue;
		if (CellMin[UAxis] >= Face.Min[UAxis] && CellMin[UAxis] + GetSampleStep() <= Face.Max[UAxis] &&
			CellMin[VAxis] >= Face.Min[VAxis] && CellMin[VAxis] + GetSampleStep() <= Face.Max[VAxis]) return true;
	}
	for (const FVoxelBoundaryTransitionPatch& Patch : Patches)
	{
		if (Patch.Face.Owner != Owner ||
			static_cast<uint8>(Patch.Face.Direction) != InFace) continue;
		const int32 Plane = (InFace & 1) ? Patch.Face.Max[Axis] : Patch.Face.Min[Axis];
		if (CellMin[Axis] + ((InFace & 1) ? 0 : GetSampleStep()) != Plane) continue;
		if (CellMin[UAxis] >= Patch.Face.Min[UAxis] &&
			CellMin[UAxis] + GetSampleStep() <= Patch.Face.Max[UAxis] &&
			CellMin[VAxis] >= Patch.Face.Min[VAxis] &&
			CellMin[VAxis] + GetSampleStep() <= Patch.Face.Max[VAxis]) return true;
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
			Owner.Level < Patch.Neighbor.Key.Level ||
			Patch.Neighbor.Face != Opposite(Face) ||
			Patch.Neighbor.States.Num() != FMath::Square(Patch.Neighbor.GridSide > 0 ? Patch.Neighbor.GridSide : Patch.Neighbor.Key.GetGridSide()) ||
			Patch.Face.Ratio < 1 || Patch.Face.Ratio > (1 << 20) ||
			!FMath::IsPowerOfTwo(Patch.Face.Ratio) ||
			Owner.GetSide() / Patch.Face.Ratio != Patch.Neighbor.Key.GetSide() ||
			((Face & 1) ? (NeighborBounds.Max[Axis] < OwnerBounds.Min[Axis] || NeighborBounds.Max[Axis] > OwnerBounds.Max[Axis])
				: (NeighborBounds.Min[Axis] < OwnerBounds.Min[Axis] || NeighborBounds.Min[Axis] > OwnerBounds.Max[Axis])) ||
			Patch.Face.Min != NeighborBounds.Min ||
			Patch.Face.Max != NeighborBounds.Max ||
			Patch.Face.Min[UAxis] < OwnerBounds.Min[UAxis] ||
			Patch.Face.Max[UAxis] > OwnerBounds.Max[UAxis] ||
			Patch.Face.Min[VAxis] < OwnerBounds.Min[VAxis] ||
			Patch.Face.Max[VAxis] > OwnerBounds.Max[VAxis])
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
			const int32 PlaneFirst = (static_cast<uint8>(First.Direction) & 1) ? First.Max[Axis] : First.Min[Axis];
			const int32 PlaneSecond = (static_cast<uint8>(Second.Direction) & 1) ? Second.Max[Axis] : Second.Min[Axis];
			if (PlaneFirst != PlaneSecond) continue;
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
	Mix(SuppressionRevision);
	for (const auto& Face : SuppressedFaces)
	{ Mix(GetTypeHash(Face.Owner)); Mix(GetTypeHash(Face.Neighbor)); Mix(static_cast<uint8>(Face.Direction)); Mix(GetTypeHash(Face.Min)); Mix(GetTypeHash(Face.Max)); }
	Mix(GetSampleStep());
	for (const FVoxelBoundaryTransitionPatch& Patch : Patches)
	{
		Mix(GetTypeHash(Patch.Face.Neighbor));
		Mix(static_cast<uint8>(Patch.Face.Direction));
		Mix(Patch.Face.Ratio);
		Mix(Patch.Neighbor.Revision);
		Mix(Patch.Neighbor.SampleStepCells);
	}
	return Hash;
}
