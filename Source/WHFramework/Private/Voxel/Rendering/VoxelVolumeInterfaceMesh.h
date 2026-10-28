#pragma once

#include "Voxel/Geometry/DWVoxelBoundaryTransition.h"
#include "Voxel/Rendering/VoxelCoarseBuildStorage.h"
#include "Voxel/Rendering/VoxelProxyPageMesh.h"

// 只细分切向执行窗口，不改变两侧真实采样步长。Count 与 Build 遍历相同冻结 Patch。
class FVoxelVolumeInterfaceMesh
{
public:
	void Initialize(const bool bInCount, const uint64 InPieceLimit, const uint64 InGeometryLimit, const uint64 InResultCapacity = 0)
	{
		bCount = bInCount; PieceLimit = InPieceLimit; GeometryLimit = InGeometryLimit; ResultCapacity = InResultCapacity;
		PatchCursor = U = V = BatchCount = 0; TileSide = 4; bPatchStarted = false; PayloadBytes = 0; Mesh = {};
	}

	uint64 EstimatedBytes() const
	{
		return PayloadBytes + VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(BatchCount);
	}

	bool Advance(const FVoxelBoundaryTransitionContext& Inputs, const FVoxelRegistrySnapshot& Registry,
		const FVoxelShapeRegistry& Shapes, FVoxelSectionMeshResult& OutMesh, bool& bComplete,
		FString& OutError, const TAtomic<bool>& Cancel)
	{
		bComplete = false;
		if (Cancel.Load()) { OutError = TEXT("Canceled"); return false; }
		if (!Inputs.Patches.IsValidIndex(PatchCursor))
		{
			if (!bCount)
			{
				if (!Mesh.ValidateAndCache()) { OutError = TEXT("Invalid volume interface mesh"); return false; }
				OutMesh = MoveTemp(Mesh);
			}
			bComplete = true; return true;
		}
		const auto& Original = Inputs.Patches[PatchCursor];
		const uint8 Face = uint8(Original.Face.Direction);
		const int32 Axis = Face / 2, TangentU = Axis == 0 ? 1 : 0, TangentV = Axis == 2 ? 1 : 2;
		if (!bPatchStarted) { U = Original.Face.Min[TangentU]; V = Original.Face.Min[TangentV]; bPatchStarted = true; }
		const int32 BatchBound = 12 * Registry.Definitions.Num();
		const uint64 Scratch = 2 * VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(BatchBound) +
			VoxelBoundedAllocation::MapBytes<uint32, int32>(BatchBound) +
			VoxelBoundedAllocation::ArrayBytes<FVoxelBoundaryTransitionPatch>(1);
		if (Scratch > PieceLimit) { OutError = TEXT("Oversized: volume interface Count metadata"); return false; }
		FVoxelBoundaryTransitionContext Local;
		Local.Owner = Inputs.Owner; Local.SampleStepCells = Inputs.SampleStepCells; Local.OwnerGeometry = Inputs.OwnerGeometry;
		Local.FineOwnerSource = Inputs.FineOwnerSource; Local.MaximumTextureStretchCells = Inputs.MaximumTextureStretchCells;
		Local.WorldEpoch = Inputs.WorldEpoch; Local.GeneratorIdentity = Inputs.GeneratorIdentity;
		Local.RegistryRevision = Inputs.RegistryRevision; Local.ShapeRevision = Inputs.ShapeRevision; Local.InputSerial = Inputs.InputSerial;
		Local.Patches.Reserve(1); Local.Patches.Add(Original);
		auto& FaceRange = Local.Patches[0].Face;
		FaceRange.Min[TangentU] = U; FaceRange.Max[TangentU] = FMath::Min(U + TileSide, Original.Face.Max[TangentU]);
		FaceRange.Min[TangentV] = V; FaceRange.Max[TangentV] = FMath::Min(V + TileSide, Original.Face.Max[TangentV]);
		FVoxelSectionSnapshot Snapshot; Snapshot.GridSide = Inputs.GetGridSide();
		FVoxelSectionMeshResult Plan;
		if (!FVoxelSectionMesher::Build(Snapshot, Registry, Shapes, Plan, &Cancel, 1.0, &Local, true, true, {}, 0, true))
		{ OutError = TEXT("Failed to count frozen volume interface"); return false; }
		const int32 AddedBatches = Plan.Batches.Num();
		const uint64 AddedPayload = Plan.PlannedBytes() - VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(AddedBatches);
		const uint64 NewBatchBytes = VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(BatchCount + AddedBatches);
		const uint64 BuildPeak = Scratch + Plan.Batches.GetAllocatedSize() + AddedPayload +
			VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(BatchCount) + NewBatchBytes + VoxelBoundedAllocation::ArrayBytes<FVoxelRenderBatch>(AddedBatches);
		if (BuildPeak > PieceLimit)
		{
			if (TileSide > 1) { TileSide /= 2; return true; }
			OutError = TEXT("Oversized: minimum volume interface Piece peak"); return false;
		}
		if (PayloadBytes + AddedPayload + NewBatchBytes > GeometryLimit)
		{ OutError = TEXT("Oversized: volume interface cumulative geometry"); return false; }
		if (!bCount)
		{
			if (PayloadBytes + AddedPayload + NewBatchBytes > ResultCapacity)
			{ OutError = TEXT("Volume interface Count/Build capacity mismatch"); return false; }
			FVoxelSectionMeshResult Part;
			if (!FVoxelSectionMesher::Build(Snapshot, Registry, Shapes, Part, &Cancel, 1.0, &Local, true, true, {}, 0, false, &Plan))
			{ OutError = TEXT("Volume interface Count/Build mismatch"); return false; }
			Mesh.Batches.Reserve(BatchCount + AddedBatches);
			for (auto& Batch : Part.Batches) Mesh.Batches.Add(MoveTemp(Batch));
		}
		PayloadBytes += AddedPayload; BatchCount += AddedBatches;
		U = FaceRange.Max[TangentU];
		if (U == Original.Face.Max[TangentU]) { U = Original.Face.Min[TangentU]; V = FaceRange.Max[TangentV]; }
		if (V == Original.Face.Max[TangentV]) { ++PatchCursor; bPatchStarted = false; }
		return true;
	}

private:
	FVoxelSectionMeshResult Mesh;
	int32 PatchCursor = 0, U = 0, V = 0, BatchCount = 0;
	int32 TileSide = 4;
	uint64 PayloadBytes = 0, PieceLimit = 0, GeometryLimit = 0, ResultCapacity = 0;
	bool bCount = false;
	bool bPatchStarted = false;
};

struct FVoxelVolumeTransitionBuild
{
	TSharedPtr<FVoxelCoarseBuildStorage, ESPMode::ThreadSafe> Storage;
	TSharedPtr<const FVoxelVoxelProxyData, ESPMode::ThreadSafe> Data;
	TSharedPtr<const FVoxelBoundaryTransitionContext, ESPMode::ThreadSafe> Inputs;
	FVoxelProxyPageMesh BodyCount, BodyBuild;
	FVoxelVolumeInterfaceMesh BoundaryCount, BoundaryBuild;
	TSharedPtr<FVoxelSectionMeshResult, ESPMode::ThreadSafe> Body;
	uint64 GeometryLimit = 0;
	uint64 BodyBytes = 0, BoundaryBytes = 0;
	int32 Phase = 0;

	FVoxelTaskResult Advance(const FVoxelRegistrySnapshot& Registry, const FVoxelShapeRegistry& Shapes,
		const TAtomic<bool>& Cancel)
	{
		FVoxelTaskResult Result;
		if (Cancel.Load()) { Result.bCanceled = true; return Result; }
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Working;
		if (!(Phase < 3 ? Storage->BeginCount(Result, Working) : Storage->Begin(Result, Working))) return Result;
		const double TextureRepeats = Data->Geometry.SampleStepCells / FMath::Clamp(double(Data->Geometry.SampleStepCells), 1.0, Inputs->MaximumTextureStretchCells);
		bool bComplete = false;
		if (Phase == 0)
		{
			if (!BodyCount.Initialize(Data->Geometry, Result.Error, 0, true, Storage->WorkingCapacityBytes, GeometryLimit)) return Result;
			BoundaryCount.Initialize(true, Storage->WorkingCapacityBytes, GeometryLimit); ++Phase;
		}
		else if (Phase == 1)
		{
			FVoxelSectionMeshResult Counted;
			if (!BodyCount.Advance(*Data, Registry, Shapes, Inputs.Get(), TextureRepeats, Counted, bComplete, Result.Error, &Cancel)) return Result;
			if (bComplete) { BodyBytes = BodyCount.GetEstimatedMeshBytes(); ++Phase; }
		}
		else if (Phase == 2)
		{
			FVoxelSectionMeshResult Counted;
			if (!BoundaryCount.Advance(*Inputs, Registry, Shapes, Counted, bComplete, Result.Error, Cancel)) return Result;
			if (bComplete)
			{
				BoundaryBytes = BoundaryCount.EstimatedBytes();
				if (BodyBytes > GeometryLimit || BoundaryBytes > GeometryLimit - BodyBytes)
				{ Result.Error = TEXT("Oversized: complete volume transition geometry"); return Result; }
				Storage->GeometryCapacityBytes = BodyBytes + BoundaryBytes +
					2 * VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionMeshResult>();
				++Phase;
			}
		}
		else if (Phase == 3)
		{
			if (!BodyBuild.Initialize(Data->Geometry, Result.Error, BodyBytes, false, Storage->WorkingCapacityBytes, GeometryLimit)) return Result;
			BoundaryBuild.Initialize(false, Storage->WorkingCapacityBytes, GeometryLimit, BoundaryBytes);
			Body = Storage->MakeResult<FVoxelSectionMeshResult>(); ++Phase;
		}
		else if (Phase == 4)
		{
			if (!BodyBuild.Advance(*Data, Registry, Shapes, Inputs.Get(), TextureRepeats, *Body, bComplete, Result.Error, &Cancel)) return Result;
			if (bComplete) { Body->ResourceLease = Storage->GeometryLease; ++Phase; }
		}
		else
		{
			auto Boundary = Storage->MakeResult<FVoxelSectionMeshResult>();
			if (!BoundaryBuild.Advance(*Inputs, Registry, Shapes, *Boundary, bComplete, Result.Error, Cancel)) return Result;
			if (bComplete)
			{
				Boundary->ResourceLease = Storage->GeometryLease;
				Body->FrozenBoundaryInputs = Inputs; Body->FrozenVoxelProxy = Data;
				Boundary->FrozenBoundaryInputs = Inputs; Boundary->FrozenVoxelProxy = Data;
				Result.VoxelProxyMesh = MoveTemp(Body); Result.VolumeBoundaryMesh = MoveTemp(Boundary);
				Result.InputResourceLease = Storage->ResultInputLease; Result.bSuccess = true;
				Storage->SettleCompleted(Result, nullptr, 0, &Cancel);
				return Result;
			}
		}
		Result.bSuccess = Result.bContinue = true;
		Result.bContinueImmediately = true;
		return Result;
	}
};
