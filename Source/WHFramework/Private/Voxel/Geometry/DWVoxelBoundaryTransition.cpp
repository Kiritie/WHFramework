#include "Voxel/Geometry/DWVoxelBoundaryTransition.h"

#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"
#include "Voxel/Runtime/VoxelSection.h"
#include "Voxel/Rendering/VoxelFineShardBuild.h"

namespace
{


	uint8 Opposite(const uint8 InFace)
	{
		return InFace ^ 1;
	}

	template<typename StorageType>
	bool CaptureStates(const FVoxelViewKey& InKey, const uint8 InFace,
		const StorageType& InBlocks, const uint64 InRevision, const int32 InStep,
		FVoxelBoundaryFaceSnapshot& OutSnapshot)
	{
		if (InStep <= 0 || !FMath::IsPowerOfTwo(InStep) || InKey.GetSide() % InStep != 0)
		{
			return false;
		}
		const int32 SectionSide = InKey.GetSide() / InStep;
		if (InFace >= 6 || int64(InBlocks.Num()) != int64(SectionSide) * SectionSide * SectionSide)
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
	FString Error;
	if (!InData.Geometry.Validate(Error) || InData.Geometry.Key.Representation != EVoxelTerrainRepresentation::VoxelProxy ||
		InData.Geometry.Key.Coordinate != InData.Key.Coordinate || InData.Geometry.Key.Level != InData.Key.Level ||
		InData.LocalMinCells != FIntVector::ZeroValue || InData.GridSide <= 0 || InData.GridSide > 128 || int64(InData.GridSide) * InData.Geometry.SampleStepCells != InData.Key.GetSide())
	{
		return false;
	}
	return CaptureStates(InData.Key, InFace, InData.Cells,
		InData.Revision, InData.Geometry.SampleStepCells, OutSnapshot);
}

bool FVoxelBoundaryFaceSnapshot::CaptureFineShardAtom(const FVoxelViewKey& InKey, const uint8 InFace,
	TSharedPtr<const FVoxelFineShardGeometry, ESPMode::ThreadSafe> InGeometry, FVoxelBoundaryFaceSnapshot& OutSnapshot)
{
	if (InKey.Level != 0 || InFace >= 6 || !InGeometry || !InGeometry->GeometryIdentity.IsValid() ||
		!InGeometry->Boundary || InGeometry->Boundary->Input != InGeometry->Attempt.Input ||
		!FVoxelFineSpatial::ContainsBounds(InGeometry->Attempt.Input.Shard.Bounds(), InKey.GetBounds())) return false;
	FVoxelBoundaryFaceSnapshot Snapshot;
	Snapshot.Key = InKey;
	Snapshot.Face = InFace;
	Snapshot.SampleStepCells = 1;
	Snapshot.GridSide = 16;
	Snapshot.Revision = InGeometry->Attempt.Input.DataRevision;
	Snapshot.FineSource = FVoxelFineBoundarySource(*InGeometry);
	uint32 Packed = 0;
	if (!Snapshot.GetPackedState(0, Packed) || !Snapshot.GetPackedState(255, Packed)) return false;
	OutSnapshot = MoveTemp(Snapshot);
	return true;
}

bool FVoxelBoundaryFaceSnapshot::CaptureProxyFrozen(TSharedPtr<const FVoxelVoxelProxyData, ESPMode::ThreadSafe> InData,
	const uint8 InFace, FVoxelBoundaryFaceSnapshot& OutSnapshot)
{
	FString Error;
	if (!InData || InFace >= 6 || !InData->Geometry.Validate(Error) ||
		InData->Geometry.Key.Representation != EVoxelTerrainRepresentation::VoxelProxy ||
		InData->Geometry.Key.Coordinate != InData->Key.Coordinate || InData->Geometry.Key.Level != InData->Key.Level ||
		InData->LocalMinCells != FIntVector::ZeroValue || InData->GridSide <= 0 || InData->GridSide > 128 || int64(InData->GridSide) * InData->Geometry.SampleStepCells != InData->Key.GetSide() ||
		int64(InData->Cells.Num()) != int64(InData->GridSide) * InData->GridSide * InData->GridSide) return false;
	FVoxelBoundaryFaceSnapshot Snapshot;
	Snapshot.Key = InData->Key;
	Snapshot.Face = InFace;
	Snapshot.SampleStepCells = InData->Geometry.SampleStepCells;
	Snapshot.GridSide = InData->GridSide;
	Snapshot.Revision = InData->Revision;
	Snapshot.ProxyGeometry = MoveTemp(InData);
	OutSnapshot = MoveTemp(Snapshot);
	return true;
}

int32 FVoxelBoundaryFaceSnapshot::GetStateCount() const
{
	if (GridSide <= 0 || GridSide > 4096 || Face >= 6) return 0;
	return GridSide * GridSide;
}

bool FVoxelBoundaryFaceSnapshot::GetPackedState(const int32 InIndex, uint32& OutPacked) const
{
	if (InIndex < 0 || InIndex >= GetStateCount()) return false;
	if (FineSource.Boundary)
	{
		if (!FineSource.GeometryIdentity.IsValid() || Key.Level != 0 || GridSide != 16 || SampleStepCells != 1 ||
			!FVoxelFineSpatial::ContainsBounds(FineSource.Boundary->Input.Shard.Bounds(), Key.GetBounds())) return false;
		FIntVector Cell = Key.GetBounds().Min;
		const int32 Axis = Face / 2, U = (Axis + 1) % 3, V = (Axis + 2) % 3;
		Cell[Axis] += (Face & 1) ? 0 : 15;
		Cell[U] += InIndex % 16; Cell[V] += InIndex / 16;
		FVoxelBlockState State;
		if (!FineSource.Boundary->SampleAtomFace(Key.Coordinate, Face, false, Cell, State)) return false;
		OutPacked = State.Pack();
		return true;
	}
	if (ProxyGeometry)
	{
		if (ProxyGeometry->Key != Key || ProxyGeometry->GridSide != GridSide ||
			ProxyGeometry->Geometry.SampleStepCells != SampleStepCells) return false;
		FIntVector Local(0);
		const int32 Axis = Face / 2, U = (Axis + 1) % 3, V = (Axis + 2) % 3;
		Local[Axis] = (Face & 1) ? 0 : GridSide - 1;
		Local[U] = InIndex % GridSide; Local[V] = InIndex / GridSide;
		const int32 Index = Local.X + GridSide * (Local.Y + GridSide * Local.Z);
		if (!ProxyGeometry->Cells.IsValidIndex(Index)) return false;
		OutPacked = ProxyGeometry->Cells[Index].Pack();
		return true;
	}
	if (!States.IsValidIndex(InIndex)) return false;
	OutPacked = States[InIndex];
	return true;
}

bool FVoxelBoundaryFaceSnapshot::Sample(const FIntVector& InWorldCell,
	FVoxelBlockState& OutState) const
{
	const int32 Step = SampleStepCells;
	const int32 SectionSide = GridSide;
	if (Step <= 0 || !FMath::IsPowerOfTwo(Step) || SectionSide <= 0 || SectionSide > 128 ||
		(Key.Level == 0 ? Step != 1 : Step < 2) ||
		int64(SectionSide) * Step != Key.GetSide() || Face >= 6 || (!FineSource.Boundary && !ProxyGeometry &&
			int64(States.Num()) != int64(SectionSide) * SectionSide)) return false;
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
	uint32 Packed = 0;
	if (!GetPackedState(U + SectionSide * V, Packed)) return false;
	OutState = FVoxelBlockState::Unpack(Packed);
	return true;
}

bool FVoxelBoundaryTransitionContext::CoversCell(const uint8 InFace,
	const FIntVector& InLocalCell) const
{
	const int32 SectionSide = GetGridSide();
	if (InFace >= 6 || SectionSide <= 0) return false;
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
	if (!FMath::IsFinite(MaximumTextureStretchCells) || MaximumTextureStretchCells < 1.0 ||
		(FineOwnerSource.Boundary && (Owner.Level != 0 || SampleStepCells != 1 || !FineOwnerSource.GeometryIdentity.IsValid() ||
			!FVoxelFineSpatial::ContainsBounds(FineOwnerSource.Boundary->Input.Shard.Bounds(), Owner.GetBounds()))) ||
		SampleStepCells <= 0 || !FMath::IsPowerOfTwo(SampleStepCells) || Owner.GetSide() % SampleStepCells != 0 ||
		(Owner.Level == 0 ? SampleStepCells != 1 : SampleStepCells < 2))
	{
		return false;
	}
	if (OwnerGeometry)
	{
		FString Error;
		if (!OwnerGeometry->Geometry.Validate(Error) ||
			OwnerGeometry->Geometry.Key != FVoxelTerrainSpatialKey{EVoxelTerrainRepresentation::VoxelProxy, Owner.Coordinate, Owner.Level} ||
			OwnerGeometry->Key != Owner || OwnerGeometry->Geometry.SampleStepCells != SampleStepCells ||
			OwnerGeometry->LocalMinCells != FIntVector::ZeroValue || OwnerGeometry->GridSide <= 0 || OwnerGeometry->GridSide > 128 || int64(OwnerGeometry->GridSide) * SampleStepCells != Owner.GetSide() ||
			int64(OwnerGeometry->Cells.Num()) != int64(OwnerGeometry->GridSide) * OwnerGeometry->GridSide * OwnerGeometry->GridSide)
		{
			return false;
		}
	}
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
			Patch.Neighbor.SampleStepCells <= 0 || !FMath::IsPowerOfTwo(Patch.Neighbor.SampleStepCells) ||
			Patch.Neighbor.GridSide <= 0 || Patch.Neighbor.GridSide > 128 || int64(Patch.Neighbor.GridSide) * Patch.Neighbor.SampleStepCells != Patch.Neighbor.Key.GetSide() ||
			(Patch.Neighbor.Key.Level == 0 ? Patch.Neighbor.SampleStepCells != 1 : Patch.Neighbor.SampleStepCells < 2) ||
			(Patch.Neighbor.FineSource.Boundary && (Patch.Neighbor.SampleStepCells != 1 || !Patch.Neighbor.FineSource.GeometryIdentity.IsValid())) ||
			(Patch.Neighbor.ProxyGeometry && Patch.Neighbor.SampleStepCells < 2) ||
			(!Patch.Neighbor.FineSource.Boundary && !Patch.Neighbor.ProxyGeometry &&
				int64(Patch.Neighbor.States.Num()) != int64(Patch.Neighbor.GridSide) * Patch.Neighbor.GridSide) ||
			Patch.Face.Ratio < 1 || Patch.Face.Ratio > (1 << 20) ||
			!FMath::IsPowerOfTwo(Patch.Face.Ratio) ||
			Owner.GetSide() / Patch.Face.Ratio != Patch.Neighbor.Key.GetSide() ||
			((Face & 1) ? (NeighborBounds.Max[Axis] < OwnerBounds.Min[Axis] || NeighborBounds.Max[Axis] > OwnerBounds.Max[Axis])
				: (NeighborBounds.Min[Axis] < OwnerBounds.Min[Axis] || NeighborBounds.Min[Axis] > OwnerBounds.Max[Axis])) ||
			Patch.Face.Min[Axis] != NeighborBounds.Min[Axis] || Patch.Face.Max[Axis] != NeighborBounds.Max[Axis] ||
			Patch.Face.Min[UAxis] < NeighborBounds.Min[UAxis] || Patch.Face.Max[UAxis] > NeighborBounds.Max[UAxis] ||
			Patch.Face.Min[VAxis] < NeighborBounds.Min[VAxis] || Patch.Face.Max[VAxis] > NeighborBounds.Max[VAxis] ||
			Patch.Face.Min[UAxis] >= Patch.Face.Max[UAxis] || Patch.Face.Min[VAxis] >= Patch.Face.Max[VAxis] ||
			Patch.Face.Min[UAxis] < OwnerBounds.Min[UAxis] ||
			Patch.Face.Max[UAxis] > OwnerBounds.Max[UAxis] ||
			Patch.Face.Min[VAxis] < OwnerBounds.Min[VAxis] ||
			Patch.Face.Max[VAxis] > OwnerBounds.Max[VAxis])
		{
			return false;
		}
	}
	for (const auto& Suppression : SuppressedFaces)
	{
		const uint8 Face = uint8(Suppression.Direction);
		if (Face >= 6 || Suppression.Owner != Owner) return false;
		const int32 Axis = Face / 2, U = (Axis + 1) % 3, V = (Axis + 2) % 3;
		const int32 Plane = (Face & 1) ? OwnerBounds.Min[Axis] : OwnerBounds.Max[Axis];
		if (Suppression.Min[Axis] != Plane || Suppression.Max[Axis] != Plane ||
			Suppression.Min[U] < OwnerBounds.Min[U] || Suppression.Max[U] > OwnerBounds.Max[U] ||
			Suppression.Min[V] < OwnerBounds.Min[V] || Suppression.Max[V] > OwnerBounds.Max[V] ||
			Suppression.Min[U] >= Suppression.Max[U] || Suppression.Min[V] >= Suppression.Max[V]) return false;
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

bool FVoxelBoundaryFaceSnapshot::SameSource(const FVoxelBoundaryFaceSnapshot& InOther) const
{
	// 指针是冻结对象的精确身份；持有强引用防止地址回收造成 ABA。
	return Key == InOther.Key && Face == InOther.Face && SampleStepCells == InOther.SampleStepCells &&
		GridSide == InOther.GridSide && Revision == InOther.Revision && FineSource == InOther.FineSource &&
		ProxyGeometry == InOther.ProxyGeometry && States == InOther.States;
}

bool FVoxelBoundaryTransitionContext::SameInputs(const FVoxelBoundaryTransitionContext& InOther) const
{
	if (Owner != InOther.Owner || SampleStepCells != InOther.SampleStepCells || OwnerGeometry != InOther.OwnerGeometry ||
		FineOwnerSource != InOther.FineOwnerSource || MaximumTextureStretchCells != InOther.MaximumTextureStretchCells ||
		WorldEpoch != InOther.WorldEpoch || GeneratorIdentity != InOther.GeneratorIdentity ||
		RegistryRevision != InOther.RegistryRevision || ShapeRevision != InOther.ShapeRevision ||
		SuppressionRevision != InOther.SuppressionRevision || Patches.Num() != InOther.Patches.Num() ||
		SuppressedFaces.Num() != InOther.SuppressedFaces.Num() || SuppressionSources != InOther.SuppressionSources) return false;
	const auto SameFace = [](const FVoxelVolumeTransitionFace& A, const FVoxelVolumeTransitionFace& B)
	{
		return A.Owner == B.Owner && A.Neighbor == B.Neighbor && A.Direction == B.Direction && A.Ratio == B.Ratio &&
			A.Min == B.Min && A.Max == B.Max;
	};
	for (int32 Index = 0; Index < Patches.Num(); ++Index)
		if (!SameFace(Patches[Index].Face, InOther.Patches[Index].Face) ||
			!Patches[Index].Neighbor.SameSource(InOther.Patches[Index].Neighbor)) return false;
	for (int32 Index = 0; Index < SuppressedFaces.Num(); ++Index)
		if (!SameFace(SuppressedFaces[Index], InOther.SuppressedFaces[Index])) return false;
	return true;
}

uint64 FVoxelBoundaryTransitionContext::Signature(const uint64 InOwnerRevision) const
{
	// 生产路径先逐字段/源引用比对，再分配不可复用 serial；hash 仅用于旧的纯数据测试输入。
	if (InputSerial != 0) return InputSerial;
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
	Mix(reinterpret_cast<UPTRINT>(OwnerGeometry.Get()));
	Mix(reinterpret_cast<UPTRINT>(FineOwnerSource.Boundary.Get()));
	Mix(GetTypeHash(FineOwnerSource.GeometryIdentity));
	for (const auto& Source : SuppressionSources) Mix(reinterpret_cast<UPTRINT>(Source.Get()));
	for (const FVoxelBoundaryTransitionPatch& Patch : Patches)
	{
		Mix(GetTypeHash(Patch.Face.Neighbor));
		Mix(static_cast<uint8>(Patch.Face.Direction));
		Mix(Patch.Face.Ratio);
		Mix(Patch.Neighbor.Revision);
		Mix(Patch.Neighbor.SampleStepCells);
		Mix(reinterpret_cast<UPTRINT>(Patch.Neighbor.FineSource.Boundary.Get()));
		Mix(GetTypeHash(Patch.Neighbor.FineSource.GeometryIdentity));
		Mix(reinterpret_cast<UPTRINT>(Patch.Neighbor.ProxyGeometry.Get()));
	}
	return Hash;
}
